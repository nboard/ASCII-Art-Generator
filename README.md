# Ascii Art Generator
This project used the opencv library to convert images into ascii art.

## Running the Project
This project was created using WSL with ubuntu. With the open CV library installed, run the following command to build it: 

`g++ GenerateAscii.cpp -I <path-to-open-cv-install>/include/opencv4 -L <path-to-open-cv-install>/lib  -l opencv_core -l opencv_imgcodecs -l opencv_highgui -l opencv_imgproc`

Then, if needed, run the following to make sure the opencv library can be found: 

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<path-to-open-cv-install>/lib/`

Finally, simply run the a.out file followed by a path to the image you would like to convert

## Notes
The current algorithm essentially works by tracing the boundaries between colors and trying to find ascii characters which best fit the outline created. This means it works best on images with few colors and sharp changes between colors (e.g. simple cartoon images). Note that having an outline around an image means the outline will be traced twice (inner color to line, line to outer color). Preprocessing images to remove outlines would improve results. 