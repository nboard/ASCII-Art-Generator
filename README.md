# Ascii Art Generator
This project used the opencv library to convert images into ascii art.

## Purpose
Most automatic ascii art generators simply average the brightness of images to create simplified images. These have their charms, but are very dissimilar to handmade ascii art, which typically outlines images much like a simple sketch. 

I was inspired by Junferno's youtube video ["How I animate stuff on Desmos Graphing Calculator"](https://www.youtube.com/watch?v=BQvBq3K50u8) to use opencv's library to produce an outline, which then can be used as a starting point for converting to ascii art which more closely mimics a handmade style. 

One project of note I came across was Osamu Akiyama's [DeepAA](https://github.com/OsciiArt/DeepAA) project, which uses machine learning to convert line art into ascii art. As of now I have not used anything from this project nor even tested it (thus, I cannot endorse it), but it may be interesting for anyone who found my project entertaining. 


## Running the Project
This project was created using WSL with ubuntu. With the open CV library installed, run the following command to build it: 

`g++ GenerateAscii.cpp -I <path-to-open-cv-install>/include/opencv4 -L <path-to-open-cv-install>/lib  -l opencv_core -l opencv_imgcodecs -l opencv_highgui -l opencv_imgproc`

Then, simply run the a.out file followed by a path to the image you would like to convert. 

Usage: `a.out filename [-d] [-b blurThreshold] [-l lowThreshold] [-r ratio] [-k kernelSize] [-h asciiHeight]`

Note that the filename MUST be the first argument, but that the order of the other arguments does not matter. 

### Argument definitions: 

`-d: Runs the program in demo mode`

`-b, --blur: Sets the blur threshold value`

`-l, --low: Sets the low threshold value`

`-r, --ratio: Sets the ratio value`

`-k, --kernel: Sets the kernel size value. Must be 3, 5, or 7`

`-c, --height: Sets the character height of the generated ascii art`

## Notes
### Getting better images
The current algorithm essentially works by tracing the boundaries between colors and trying to find ascii characters which best fit the outline created. This means it works best on images with few colors and sharp changes between colors (e.g. simple cartoon images). Note that having an outline around an image means the outline will be traced twice (inner color to line, line to outer color). Preprocessing images to remove outlines would improve results. 
### On the programming style
Although this project is written in C++ (in large part because that is one of the languages the opencv library is available in), it is programmed more like a C program. This is simply because I wanted more practice with C programming, though I do take advantage of some features of C++. My apologies in advance for any headaches this may cause.