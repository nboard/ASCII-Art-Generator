#pragma once
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
using namespace cv;

// constants
const double LEN_WID_RATIO 	= 2.2; // how many character widths equal one character height (2.2 is my vs console)
const std::string WINDOW_NAME 	= "Demo Mode";
const int MAX_BLUR_THRESHOLD 	= 50;
const int MAX_LOW_THRESHOLD	= 100;
const int MAX_RATIO 		= 100;
const int MAX_ASCII_HEIGHT	= 100;

// function declarations
bool isWhite(Mat detectedEdges, int xMin, int xMax, int yMin, int yMax);
static void simpleReplace(int ascHeight, int ascWidth, char* result, char* giant);
static char * outlineToAscii(Mat src, int ascHeight);
void CannyThreshold(int, void*);
void demoImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight);
char* convertImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight);