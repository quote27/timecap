Timecap
====================

Timecap is a screen shot recording tool to help gather visual response times.  Specifically, the
tool will record an area of the screen and output images whenever the area changes along with a
timestamp.

This was initially developed to help gather reportable load times for web pages that continue to
initialize after onload is triggered.  Tools like Firebug or Chrome's Developer Tools did not always
report accurate times in these scenarios due to overhead that inflate the results or difficulty in
gathering an accurate result (for pages that load dynamic elements). 


Example use case
----------
You have a dashboard with multiple tabs.  Clicking on a tab causes the previous tab to clean up, and
makes several async calls to fetch new data.  The length of time it takes to switch [including front
end & backend processing times] can be gathered with the tool.

1. Start the recorder
2. Perform actions
3. Wait for recorder to finish

A set of images files will be saved to disk along with a console printout of image id - screen shot
time from the start.  Simply look through the frames and enter the start / end id into the tool to
get the elapsed time.


Usage
----------
1. Load up the tool in the command line
2. Move the window that will be recorded to the top left [makes finding the dimensions easier]
3. Enter the (x,y) start position [top left is (0,0)] and the width and height of the capture area
   - Check the output image `shot0.bmp` to see if the right area was captured
   - Type `n` to retry if needed
4. Enter total capture time - how long you want the tool to run, useful if you want to capture
   multiple runs in one recording
5. Enter time step between captures - the frame rate essentially.  100ms is a good default
   - Note: frames will only be saved when the tool detects a visual change
6. Wait for the countdown, then start performing actions.  In case the estimated time was too high,
   simply wait for the tool to finish - as long as the range is static, no extra images will be
   saved
7. The console will output the time results in the form `id    time(ms)`
8. Look through the output images and enter the start and end frame ids into the tool
   - The elapsed time will be calculated and the average will be adjusted.
   - Multiple ranges can be inserted within a single capture
   - Type `0 0` to end
   - Note: images are overwritten each run
9. There are several options next:
    a. `c` will loop back to 6 to continue - averages are saved
    b. `r` will clear the results and loop back to 6
    c. `q` will print the results and quit
10. Can delete all *.bmp files to cleanup the current directory

