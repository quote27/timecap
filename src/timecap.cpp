#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>
#include <list>
#include <vector>
#include <ctime>
#include "md5.h"
using namespace std;

int _x = 0, _y = 0;
int _w = 200;
int _h = 200;
int _bmp_size = -1; // set by MatchData::init() with bmiHeader.biSizeImage
bool _running = true;


// Timer: uses QueryPerformanceCounter for timing info in milliseconds
// usage:
//   Timer a;
//   a.start();
//   ... // do stuff
//   a.stop();
//   double length = a.elapsedms();
class Timer {
    LARGE_INTEGER s, e; //start and end results

public:
    static LARGE_INTEGER freq; // results are based on cpu frequency
    static LARGE_INTEGER init_frequency() {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }

    inline void start() { QueryPerformanceCounter(&s); }

    inline double stop() { QueryPerformanceCounter(&e); return elapsedms(); }

    inline double elapsedms() const {
        LARGE_INTEGER ticks_elapsed;
        ticks_elapsed.QuadPart = e.QuadPart - s.QuadPart;
        double t = (double)ticks_elapsed.QuadPart / (double)freq.QuadPart; // time in nanoseconds
        return t * 1e3; // convert to milliseconds
    }
};
LARGE_INTEGER Timer::freq = Timer::init_frequency();


// ScreenShot: screen shot element - contains bitmap, id, and time shot was captured
struct ScreenShot {
    const HBITMAP b; // bitmap of shot
    const int id;    // id of shot
    const double t;  // time (in ms) shot was taken relative to beginning of recording

    ScreenShot(const HBITMAP _b, const int _id, const double _t) : b(_b), id(_id), t(_t) {}

    // TODO: figure out a way to keep track of the screen shots with pointers?
    // del: as ScreenShot objects are copied into the vector, they'll be destructed automatically. have to manually delete the object for now
    inline void del() { DeleteObject(b); }

    // TODO: rename / return a string and have someone else print?
    // json: md5hash of the buffer, used for html time chooser
    void json(ostream &o) const {
        uint8_t hash[16];
        md5((uint8_t*)b, 0 /*_bmp_size*/, hash);

        o<<"{\"id\":"<<id<<", \"md5\":\""<<hex;
        for(int z=0; z<16; z++)
            o << (int)hash[z];
        o<<dec<<"\", \"t\":"<<t<<"}";
    }

    // save_img: write to file fn
    bool save_img(const char *fn) const {
        HDC hdc = GetDC(0);

        LPVOID buf;
        BITMAPINFO i;
        memset(&i, 0, sizeof(BITMAPINFO));

        i.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        GetDIBits(hdc, b, 0, 0, NULL, &i, DIB_RGB_COLORS); //fill out bitmapinfo struct
        i.bmiHeader.biCompression = BI_RGB;
        if(i.bmiHeader.biSizeImage <= 0)
            i.bmiHeader.biSizeImage = _w * _h * (i.bmiHeader.biBitCount + 7) / 8;

        if((buf = malloc(i.bmiHeader.biSizeImage)) == 0) {
            cerr<<"unable to allocate memory for "<<fn<<endl;
            return false;
        }
        GetDIBits(hdc, b, 0, i.bmiHeader.biHeight, buf, &i, DIB_RGB_COLORS);

        BITMAPFILEHEADER fh;
        fh.bfType = 0x4d42; // hex for 'BM';
        fh.bfReserved1 = fh.bfReserved2 = 0;
        fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        fh.bfSize = fh.bfOffBits + i.bmiHeader.biSizeImage;

        FILE *fp = 0;
        if(fopen_s(&fp, fn, "wb")) {
            cerr<<"unable to open file for "<<fn<<endl;
            return false;
        }

        fwrite(&fh, sizeof(BITMAPFILEHEADER), 1, fp);
        fwrite(&i.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
        fwrite(buf, i.bmiHeader.biSizeImage, 1, fp);

        ReleaseDC(0, hdc);
        free(buf);
        fclose(fp);
        return true;
    }

};
list<ScreenShot> ss; // list of different screen shots captured during current run


// MatchData: stores buffers and bitmap info for current snapshot set and performs compares.
struct MatchData {
    // internally, there are 2 buffers that are swapped - last and current
    // every snapshot is compared to the last buffer using memcmp
    // if the new snapshot is different, then the screen has changed
    // after adding the image to the list, the buffers here will be swapped for the next comparison

    LPVOID lbuf, cbuf; // image buffers - last buffer, current buffer
    BITMAPINFO i;      // bitmap information
    HDC hdc;           // persistant hdc (hardware device context)

    MatchData() : lbuf(0), cbuf(0), hdc(0) { memset(&i, 0, sizeof(BITMAPINFO)); }
    ~MatchData() { reset(); }

    // reset: zeroes bitmap info, frees memory and releases device context
    void reset() {
        memset(&i, 0, sizeof(BITMAPINFO));
        if(lbuf) { free(lbuf); lbuf = 0; }
        if(cbuf) { free(cbuf); cbuf = 0; }
        if(hdc)  { ReleaseDC(0, hdc); }
    }

    // init: create dc, initializes bitmap info using 'a' as reference, allocate memory
    void init(const HBITMAP a) {
        reset();
        hdc = GetDC(0);

        i.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        GetDIBits(hdc, a, 0, 0, NULL, &i, DIB_RGB_COLORS);
        i.bmiHeader.biCompression = BI_RGB;
        if(i.bmiHeader.biSizeImage <= 0)
            i.bmiHeader.biSizeImage = _w * _h * (i.bmiHeader.biBitCount + 7) / 8;

        _bmp_size = i.bmiHeader.biSizeImage;
        lbuf = malloc(_bmp_size);
        cbuf = malloc(_bmp_size);
    }

    // load: loads bitmap into specified buffer. buf: 0 = last buffer, 1 = current buffer
    inline void load(const HBITMAP a, int buf) {
        GetDIBits(hdc, a, 0, _h, (buf==0?lbuf:cbuf), &i, DIB_RGB_COLORS);
    }

    // swapbuf: sets the last image buffer 
    inline void swapbuf() {
        LPVOID tb = cbuf;
        cbuf = lbuf;
        lbuf = tb;
    }

    // cmp: load image into current buffer [cbuf], and compare against img in last buffer [lbuf]
    inline bool cmp(const HBITMAP b) {
        load(b, 1);
        return memcmp(lbuf, cbuf, _bmp_size) == 0;
    }
};
MatchData md;


// Stats: timing info / average for kept results
struct Stats {
    vector<double> times; // times are in ms
    double avg;

    // update: prompt user for time to keep, add to list and print new average
    void update() {
        unsigned s, e, z;
        const unsigned size = ss.size();
        double st, et;
        list<ScreenShot>::iterator li;
        while(true) {
            do {
                cout<<"select start and end idx [\'0 0\' to skip]: ";
                cin>>s; cin>>e; getchar(); // TODO: clean up input - I think getchar() is needed to clear the newline
                if(s == e && s == 0) return;
            } while(s<0 || s>=size || e<0 || e>=size || s>e);

            // iterate through list, pulling out times (ms) for end and start
            z = 0;
            li = ss.begin();
            while(z++ != s) li++; // fast forward to start elem
            st = li->t; z--;
            while(z++ != e) li++; // fast forward to end elem
            et = li->t;

            times.push_back(et-st);
            average();
        }
    }

    // average: print out all the chosen times along with their averages
    void average() {
        cout<<"times["<<times.size()<<"]: ";
        avg = 0;
        for(unsigned z=0; z<times.size(); z++) {
            cout<<times[z]<<" ";
            avg += times[z];
        }
        avg /= (long double)times.size();
        cout<<"\naverage: "<<avg<<endl;
    }

    void reset() {
        avg = 0;
        times.clear();
    }
};
Stats st; // global stats object


// screencap_time: take screenshots every ms milliseconds, keeping only the unique shots
void screencap_time(const long double ms, const int sleep_time) {
    HWND deskw = GetDesktopWindow();
    HDC deskdc = GetDC(deskw);
    HDC capdc = CreateCompatibleDC(deskdc);

    HBITMAP curr = 0;
    HBITMAP last = 0;

    cout<<"screencap time: "<<ms<<"ms, step: "<<sleep_time<<endl;
    int idc = 0; // id counter
    int slp = 0; // sleep time
    Timer t, l;
    t.start();
    l.start();

    // initial picture
    curr = CreateCompatibleBitmap(deskdc, _w, _h);
    SelectObject(capdc, curr);
    BitBlt(capdc, 0, 0, _w, _h, deskdc, _x, _y, SRCCOPY|CAPTUREBLT);
    t.stop();

    md.init(curr); // needed for match to work
    ss.push_back(ScreenShot(curr, idc++, t.elapsedms()));
    last = curr;
    curr = CreateCompatibleBitmap(deskdc, _w, _h);
    md.load(curr, 0);
    t.stop();
    if((slp = sleep_time - (int)t.elapsedms()) > 0)
        Sleep(slp);

    bool savedlast = false;
    while(l.stop() <= ms) {
        t.start();
        SelectObject(capdc, curr);
        BitBlt(capdc, 0, 0, _w, _h, deskdc, _x, _y, SRCCOPY|CAPTUREBLT);
        t.stop();

        // cout<<(id++)<<" "<<idc<<" "<<(te-ls)*fm<<" "<<(te-ts)*fm<<hex<<"\tcurr:"<<curr<<" last:"<<last<<dec<<" ";
        if(!md.cmp(curr)) { //found a different shot, store and init new bitmap
            ss.push_back(ScreenShot(curr, idc++, l.stop()));
            last = curr;
            md.swapbuf();
            curr = CreateCompatibleBitmap(deskdc, _w, _h);
            savedlast = true;
        } else {
            savedlast = false; // small memory leak, might not clear 'curr' buffer correctly
        }

        if((slp = sleep_time - (int)t.stop()) > 0)
            Sleep(slp);
    }
    l.stop();
    if(!savedlast) DeleteObject(curr); // last will be deleted in the loop below

    // string for json response
    fstream jout;
    jout.open("data.json", ios_base::out);
    jout<<"[\n";

    // save shots and print out times
    for(list<ScreenShot>::iterator li=ss.begin(); li!=ss.end(); ++li) {
        ScreenShot *s = &(*li);
        cout<<s->id<<"\t"<<s->t<<endl; //"\t"<<hex<<s->b<<dec<<"\n";

        ostringstream oss;
        oss<<"shot"<<setfill('0')<<setw(4)<<s->id<<".bmp";
        s->save_img(oss.str().c_str());

        s->json(jout); jout<<",\n";
        s->del(); // delete bitmap from memory
    }

    jout<<"]"<<endl;
    jout.flush();
    jout.close();

    ReleaseDC(deskw, deskdc);
    DeleteDC(capdc);
}

// select_range: query user to find the values of the screen capture box
void select_range() {
    // TODO: cleanup input handling
    // these are the max x/y values possible
    int xmax = GetSystemMetrics(SM_CXSCREEN);
    int ymax = GetSystemMetrics(SM_CYSCREEN);

    char ans = 'n';
    while(ans == 'n') {
        cout<<"enter dimensions in the format: x y width height: ";
        cin>>_x; cin>>_y; cin>>_w; cin>>_h; getchar();

        if(_x+_w >= xmax || _y+_h >= ymax) {
            cout<<"out of bounds: max ["<<xmax<<","<<ymax<<"], input ["<<_x+_w<<","<<_y+_h<<"]"<<endl;
            continue;
        }

        screencap_time(0, 10);
        cout<<"printing image... check shot0.bmp"<<endl;
        cout<<"is this ok? (y/n) ";
        cin>>ans;
        ss.clear();
    }
}

void menu() {
    // http://stackoverflow.com/questions/903221/c-press-enter-to-continue
    // TODO: cin.get() to skip waiting for EOL
    st.update();
    char opt = 'c';
    cout<<"[c]ontinue, [r]eset, [q]uit: ";
    cin>>opt; getchar();

    switch(opt) {
        case 'r': st.reset(); break;
        case 'q': _running = false; break;
    }
}

int main(int argc, char args[]) {
    char tnows[27]; // to print out the current date
    time_t tnow = time(0);
    ctime_s(tnows, 27, &tnow);

    cout<<"timecap v0.0.5 [140702]"<<endl;
    cout<<"move window to top left (0,0) to help with dimension selection"<<endl;

    // cout<<"select top left corner and bottom right corner of capture area"<<endl;
    // TODO: 9/10/12 - pause this path for a while, switch to having user input the x,y,h,w values, repeat till right section is captured, and implement the core logic [timer]
    // bool succeed = grab_bounds();

    select_range();
    cout<<"final dimensions: x: "<<_x<<", y: "<<_y<<", w: "<<_w<<" h: "<<_h<<endl;

    cout<<"enter max time to capture (milliseconds): ";
    int time_shots = 1; cin>>time_shots;

    cout<<"enter capture time step (milliseconds): ";
    int sleep_time = 50; cin>>sleep_time;

    getchar(); //TODO: bug? without this it skips over the next getchar on 392 [need to figure out what line this was]

    while(_running) {
        cout<<"starting in... 3 "; Sleep(1000);
        cout<<"2 "; Sleep(1000);
        cout<<"1 "; Sleep(1000);
        cout<<"0 - ";
        tnow = time(0);
        ctime_s(tnows, 27, &tnow);
        cout<<tnows;

        screencap_time(time_shots, sleep_time);
        menu();
        ss.clear();
    }
    st.average();

    return 0;
}

