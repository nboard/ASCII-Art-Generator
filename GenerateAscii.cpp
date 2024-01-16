#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <vector>
#include <utility> 
#include <filesystem>
#include <queue>
// #define DEBUG_MODE
using namespace cv;

// constants
const double LEN_WID_RATIO 	= 2.2; // how many character widths equal one character height (2.2 is my vs console)
const char* WINDOW_NAME 	= "Demo Mode";
const int MAX_BLUR_THRESHOLD 	= 50;
const int MAX_LOW_THRESHOLD	= 100;
const int MAX_RATIO 		= 100;
const int MAX_ASCII_HEIGHT	= 100;
// globals for demo
int demoBlurThreshold;
Mat demoSrcGray;
Mat demoDetectedEdges;
int demoLowThreshold;
int demoRatio;
int demoKernalSize;
int demoAsciiHeight;

/**************************************
 * Helper Functions *******************
 **************************************/
// helper functions primarily used by ascii identification functions

/* isWhite: determines if there are enough white pixels in a region to consider it white. 
 * A region is considered white if it has at least as many white pixels as the regions width.
 * args:
 *	Mat detectedEdges: an image with white pixels tracing the image on a black background
 *	int xMin: minimum x pixel coordinate to check
 *	int xMax: maximum x pixel coordinate to check
 *	int yMin: minimum y pixel coordinate to check
 *	int yMax: maximum y pixel coordinate to check
*/ 
bool isWhite(Mat detectedEdges, int xMin, int xMax, int yMin, int yMax) {
	float lit = 0;
	float all = 0;
	
	// do some quick sanity checks
	if (xMin >= detectedEdges.cols) return false;
	if (yMin >= detectedEdges.rows) return false;
	if (xMax > detectedEdges.cols) xMax = detectedEdges.cols;
	if (yMax > detectedEdges.rows) yMax = detectedEdges.rows;

	// perform the check
	for (int x = xMin; x < xMax; x++) {
		for (int y = yMin; y < yMax; y++) {
			if (detectedEdges.at<char>(y, x) != 0) {
				lit++;
			}
			all++;
		}
	}
	if (lit > 0) {
	}
	return (lit / all) >= (xMax-xMin)/all;
}

/**************************************
 * Ascii Identification ***************
 **************************************/
// These functions convert the pixels in a region into one ascii character

// always returns the # character. Good to get rough outlines of images. Only good for huge images
char constChar(int xMin, int xMax, int yMin, int yMax) {
	return '#';
}

// TODO: Try an actual machine learning option -- use either feature recognition, ascii recognition if it has it, or even train my own if I can
// TODO: a method that only looks for lines, just in a lot of detail (so handling up to like x^3 for ~ or something) could work well too. Perhaps HoughLinesP() -- returns a list of the lines found. So U would be like 3 lines?
// TODO: find a better way of using convolution to replace the (removed) region matching 


// uses a breadth-first search to trace consecutive outlines. 
// FIXME: tweak to read off of a double sized pixel grid and return the normal one
// FIXME: not used
//static void breadthFirstTrace(char* result, int ascWidth, int ascHeight, int xStart, int pixWidth, int yStart, int pixHeight) {
//	std::queue<std::pair<int, int>> pos;
//
//	// make sure the current space is not blank
//	if (!isWhite(xStart * pixWidth, (xStart + 1) * pixWidth, yStart * pixHeight, (yStart + 1) * pixHeight)) {
//		result[xStart + yStart * ascWidth] = ' ';
//		return; // no line to trace
//	}
//
//	pos.push(std::pair<int, int>(xStart, yStart));
//
//	while (!pos.empty()) {
//		std::pair<int, int> cur = pos.front();
//		// TODO: once this works, replace some stuff with macros to make things cleaner
//
//		// check all around
//		// '\0' means unset, unchecked
//		// '\1' means unset, checked
//		// any other character is final, leave alone
//		for (int xOff = -1; xOff <= 1; xOff++) {
//			for (int yOff = -1; yOff <= 1; yOff++) {
//				std::pair<int, int> temp = std::pair<int, int>(cur.first + xOff, cur.second + yOff);
//				if (temp.first < 0 || temp.first >= ascWidth-1 || temp.second < 0 || temp.second >= ascHeight) continue; // invalid, ignore
//				if (result[temp.first + temp.second * ascWidth] != '\n') continue; // was checked before at some point
//				// is valid and was not checked. If is lit, mark and add. If unlit, make ' '
//				if (isWhite(temp.first * pixWidth, (temp.first + 1) * pixWidth, temp.second * pixHeight, (temp.second + 1) * pixHeight)) {
//					result[temp.first + temp.second * ascWidth] = 1;
//					pos.push(temp);
//				}
//				else result[temp.first + temp.second * ascWidth] = ' ';
//			}
//		}
//		// TODO: pick shape based on neighbors... (is 2^8 possibilities, but just check a few, mainly for /-\|. Some patterns may suggest should just skip)
//			// if you want to get real fancy, could even consider whether there are pixels clustered in the 8 spots and only link with those where that is true (maybe too complex...?)
//		result[cur.first + cur.second * ascWidth] = '#';
//		pos.pop();
//	}
//}

// scale the image down. Are just 16 possible items to translate (assumes 2x each dimension)
static void simpleReplace(int ascHeight, int ascWidth, char* result, char* giant) {
	int giantAscWidth = ascWidth * 2 - 1;
	int giantAscHeight = ascHeight * 2;
	for (int y = 0; y < ascHeight; y++) {
		for (int x = 0; x < ascWidth - 1; x++) {
			// encode the 4 values: 
			char lit = '\x00';
			if (giant[(2 * x) + (2*y) * giantAscWidth] == '#') lit |= '\x01';
			if (giant[(2 * x + 1) + (2 * y) * giantAscWidth] == '#') lit |= '\x02';
			if (giant[(2 * x) + (2 * y + 1) * giantAscWidth] == '#') lit |= '\x04';
			if (giant[(2 * x + 1) + (2 * y + 1) * giantAscWidth] == '#') lit |= '\x08';

			// select character
			switch (lit) {
				case '\x00': // |  |
							 // |  |
					result[x + y * ascWidth] = ' '; 
					break;
				case '\x01': // |# |
							 // |  |
					result[x + y * ascWidth] = '`';
					break;
				case '\x02': // | #|
							 // |  |
					result[x + y * ascWidth] = '\'';
					break;
				case '\x03': // |##|
							 // |  |
					result[x + y * ascWidth] = '-';
					break;
				case '\x04': // |  |
							 // |# |
					result[x + y * ascWidth] = '.';
					break;
				case '\x05': // |# |
							 // |# |
					result[x + y * ascWidth] = '|';
					break;
				case '\x06': // | #|
							 // |# |
					result[x + y * ascWidth] = '/';
					break;
				case '\x07': // |##|
							 // |# |
					result[x + y * ascWidth] = 'F';
					break;
				case '\x08': // |  |
							 // | #|
					result[x + y * ascWidth] = '.';
					break;
				case '\x09': // |# |
							 // | #|
					result[x + y * ascWidth] = '\\';
					break;
				case '\x0A': // | #|
							 // | #|
					result[x + y * ascWidth] = '|';
					break;
				case '\x0B': // |##|
							 // | #|
					result[x + y * ascWidth] = '7';
					break;
				case '\x0C': // |  |
							 // |##|
					result[x + y * ascWidth] = '_';
					break;
				case '\x0D': // |# |
							 // |##|
					result[x + y * ascWidth] = 'L';
					break;
				case '\x0E': // | #|
							 // |##|
					result[x + y * ascWidth] = 'j';
					break;
				case '\x0F': // |##|
							 // |##|
					result[x + y * ascWidth] = '#';
					break;
			} // switch
		} // for x
		result[(y + 1) * ascWidth - 1] = '\n';
	} // for y

	result[ascHeight * ascWidth - 1] = '\0';

	#ifdef DEBUG_MODE
		printf("%s\n", (char*)result);
	#endif
}


/**************************************
 * Image Processing *******************
 **************************************/
// these functions (well, function) use an ascii identification function convert a preprocessed image into ascii art 

/* Divides the image up into regions to be handled by an ascii identification function. Prints out the final result 
*/
// FIXME: make it return the final array
// FIXME: remove any printing done outside of debug mode
// FIXME: remove reliance on globals
static char * outlineToAscii(Mat src, int ascHeight) {
	// Create the grid for the art. Start with heigh and calculate the width
	// TODO: look into how much this warps the image by rounding
	int ascWidth = (int)(LEN_WID_RATIO * (double)(ascHeight) * (((double)src.cols) / ((double)src.rows)));
	int giantAscWidth = ascWidth * 2;
	int giantAscHeight = ascHeight * 2;

	// figure out how many pixels to each character -- use double width and double height.
	// Add to the pixel width to be sure all lines are seen, and then be careful not to read nonexistant pixels later
	//int pixHeight = (detected_edges.rows / ascHeight) + 1;
	//int pixWidth = (detected_edges.cols / ascWidth) + 1; 
	int pixHeight = (src.rows / giantAscHeight) + 1;
	int pixWidth = (src.cols / giantAscWidth) + 1;

	// Now make a grid of that length (final) and double in both dimensions (first pass)
	ascWidth++; // add an extra for the end lines
	giantAscWidth++;
	char * giantAsc = (char*)malloc(sizeof(char) * giantAscHeight * giantAscWidth);
	char* ascArt = (char*)malloc(sizeof(char) * ascHeight * ascWidth);
	memset(giantAsc, '\0', (giantAscHeight * giantAscWidth));
	memset(ascArt, '\0', ((int)ascHeight) * (ascWidth)); // TODO: figure out how to make this stop being so whiny
	
	// perform first pass; create the 2x image
	// note: for (x,y), (0,0) is the upper left, (1,1) is one right and one down, etc.
	for (int y = 0; y < giantAscHeight; y++) {
		for (int x = 0; x < giantAscWidth - 1 ; x++) {
			// just determine if it is lit
			if (isWhite(src, x*pixWidth, (x + 1) * pixWidth, y*pixHeight, (y+1)*pixHeight)) {
				giantAsc[x + y * giantAscWidth] = '#';
			}
			else {
				giantAsc[x + y * giantAscWidth] = ' ';
			}
		}
		giantAsc[(y + 1) * giantAscWidth - 1] = '\n';
	}
	giantAsc[giantAscHeight * giantAscWidth - 1] = '\0';
	#ifdef DEBUG_MODE
		printf("%s\n", (char*)giantAsc);
	#endif
	// now go through each section and condense into one char
	simpleReplace(ascHeight, ascWidth, ascArt, giantAsc);


	// free the ascii data
	free(giantAsc);
	return ascArt;
}

/**************************************
 * Opencv wrappers ********************
 **************************************/
// wrapper functions for the opencv library functions to make things simpler


static void CannyThreshold(int, void*)
{
	if (demoBlurThreshold == 0) demoBlurThreshold = 1;
	blur(demoSrcGray, demoDetectedEdges, Size(demoBlurThreshold, demoBlurThreshold));
	Canny(demoDetectedEdges, demoDetectedEdges, demoLowThreshold, demoLowThreshold * demoRatio, demoKernalSize);
	imshow(WINDOW_NAME, demoDetectedEdges);
	//print the result
	std::cout << outlineToAscii(demoDetectedEdges, demoAsciiHeight) << std::endl;
}

/* demoImage: display an image and allow users to tweak the settings so they know what to specify later */
void demoImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernalSize, int ascHeight) {
	Mat src, srcGray, detectedEdges;
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image!\n" << std::endl;
		std::cout << "Usage: " << "<THIS-FILE>" << " <Input image>" << std::endl;
		return;
	}
	//set up for processing
	cvtColor(src, srcGray, COLOR_BGR2GRAY);

	//init demo's global values
	demoBlurThreshold = blurThreshold;
	demoSrcGray = srcGray;
	demoDetectedEdges = detectedEdges;
	demoLowThreshold = lowThreshold;
	demoRatio = ratio;
	demoKernalSize = kernalSize;
	demoAsciiHeight = ascHeight;

	//show it off
	namedWindow(WINDOW_NAME, WINDOW_NORMAL);
	createTrackbar("Min Threshold:", WINDOW_NAME, &demoLowThreshold, MAX_LOW_THRESHOLD, CannyThreshold);
	createTrackbar("Blur Threshold:", WINDOW_NAME, &demoBlurThreshold, MAX_BLUR_THRESHOLD, CannyThreshold);
	createTrackbar("ratio:", WINDOW_NAME, &demoRatio, MAX_RATIO, CannyThreshold);
	createTrackbar("size:", WINDOW_NAME, &demoAsciiHeight, MAX_ASCII_HEIGHT, CannyThreshold);
	CannyThreshold(0, 0);

	waitKey(0);
}

// convert an image to ascii and return the result as a character array
char* convertImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernalSize, int ascHeight){
	Mat src, srcGray, detectedEdges;
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image " << fileName << std::endl;
		std::cout << "Usage: " << "<THIS-FILE>" << " <Input image>" << std::endl;
		return NULL;
	}

	//set up for processing
	cvtColor(src, srcGray, COLOR_BGR2GRAY);

	// process image
	if (blurThreshold == 0) blurThreshold = 1;
	blur(srcGray, detectedEdges, Size(blurThreshold, blurThreshold));
	Canny(detectedEdges, detectedEdges, lowThreshold, lowThreshold * ratio, kernalSize);
	return outlineToAscii(detectedEdges, ascHeight);
}


//Mat src, src_gray;
//Mat dst, detected_edges, white;
//// demo vars need to be global (TODO: double check this)
//int lowThreshold = 21;
//int blurThreshold = 3;
//int ascHeight = 20;


//int ratio = 4;
//const int kernel_size = 3;

//
//double lenWidRatio = 2.2; // how many character widths equal one character height (2.2 is my vs console)

/* main function *************************************************************/
// currently being used for testing. TODO: move to another file so this can more easily be used as a library

int main(int argc, char** argv)
{

	// for now, just focus on this
	if(argc <= 1){
		std::cout << "ERROR: please enter at least one image file to convert to ascii" << std::endl;
		return -1;
	}
	
	bool isDemo = false;
	// iterate through args and set values accordingly
	for(int i = 2 ; i < argc ; i++){
		if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
			// ignore everything else to print help message
			std::cout << "Usage: " << argv[0] << " filename [-s heightSize] [-b blurThreshold] [-d] " << std::endl;
			std:: cout << "this program converts images into ascii art. The first argument MUST be the filename.";
			std:: cout << " Order of other arguments does not matter. " << std::endl;
			// TODO: detail everything as I add it... Just sets the default for demo, or actual for the normal.
			return 0;
		}
		else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--demo")){
			isDemo = true;
		}
	}

	char * result;
	// determine if should demo or not
	if(isDemo) demoImage(argv[1], 3, 21, 4, 3, 20);
	else{
		result = convertImage(argv[1], 3, 21, 4, 3, 20);
		std::cout << result << std::endl;
		free(result);
	} 
 	return 0; 
}