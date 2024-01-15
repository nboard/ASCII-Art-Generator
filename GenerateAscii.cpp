#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <vector>
#include <utility> 
#include <filesystem>
#include <queue>
// #define DEBUG_MODE
using namespace cv;

Mat src, src_gray;
Mat dst, detected_edges, white;
int lowThreshold = 21;
int blurThreshold = 3;
int ascHeight = 20;
int maxAscHeight = 100;
const int max_blurThreshold = 50;
const int max_lowThreshold = 100;
const int max_ratio = 100;
int ratio = 4;
const int kernel_size = 3;
const char* window_name = "Edge Map";

double lenWidRatio = 2.2; // how many character widths equal one character height (2.2 is my vs console)

/**************************************
 * Helper Functions *******************
 **************************************/
// helper functions primarily used by ascii identification functions

/* isWhite: determines if there are enough white pixels in a region to consider it white. 
 * A region is considered white if it has at least as many white pixels as the regions width.
 * args:
 *	int xMin: minimum x pixel coordinate to check
 *	int xMax: maximum x pixel coordinate to check
 *	int yMin: minimum y pixel coordinate to check
 *	int yMax: maximum y pixel coordinate to check
*/ 
// FIXME: needs to have detected_edges passed in rather than global
bool isWhite(int xMin, int xMax, int yMin, int yMax) {
	float lit = 0;
	float all = 0;
	
	// do some quick sanity checks
	if (xMin >= detected_edges.cols) return false;
	if (yMin >= detected_edges.rows) return false;
	if (xMax > detected_edges.cols) xMax = detected_edges.cols;
	if (yMax > detected_edges.rows) yMax = detected_edges.rows;

	// perform the check
	for (int x = xMin; x < xMax; x++) {
		for (int y = yMin; y < yMax; y++) {
			if (detected_edges.at<char>(y, x) != 0) {
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
static void breadthFirstTrace(char* result, int ascWidth, int ascHeight, int xStart, int pixWidth, int yStart, int pixHeight) {
	std::queue<std::pair<int, int>> pos;

	// make sure the current space is not blank
	if (!isWhite(xStart * pixWidth, (xStart + 1) * pixWidth, yStart * pixHeight, (yStart + 1) * pixHeight)) {
		result[xStart + yStart * ascWidth] = ' ';
		return; // no line to trace
	}

	pos.push(std::pair<int, int>(xStart, yStart));

	while (!pos.empty()) {
		std::pair<int, int> cur = pos.front();
		// TODO: once this works, replace some stuff with macros to make things cleaner

		// check all around
		// '\0' means unset, unchecked
		// '\1' means unset, checked
		// any other character is final, leave alone
		for (int xOff = -1; xOff <= 1; xOff++) {
			for (int yOff = -1; yOff <= 1; yOff++) {
				std::pair<int, int> temp = std::pair<int, int>(cur.first + xOff, cur.second + yOff);
				if (temp.first < 0 || temp.first >= ascWidth-1 || temp.second < 0 || temp.second >= ascHeight) continue; // invalid, ignore
				if (result[temp.first + temp.second * ascWidth] != '\n') continue; // was checked before at some point
				// is valid and was not checked. If is lit, mark and add. If unlit, make ' '
				if (isWhite(temp.first * pixWidth, (temp.first + 1) * pixWidth, temp.second * pixHeight, (temp.second + 1) * pixHeight)) {
					result[temp.first + temp.second * ascWidth] = 1;
					pos.push(temp);
				}
				else result[temp.first + temp.second * ascWidth] = ' ';
			}
		}
		// TODO: pick shape based on neighbors... (is 2^8 possibilities, but just check a few, mainly for /-\|. Some patterns may suggest should just skip)
			// if you want to get real fancy, could even consider whether there are pixels clustered in the 8 spots and only link with those where that is true (maybe too complex...?)
		result[cur.first + cur.second * ascWidth] = '#';
		pos.pop();
	}
}

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
	printf("%s\n", (char*)result);
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
static void outlineToAscii() {
	// Create the grid for the art. Start with heigh and calculate the width
	// TODO: look into how much this warps the image by rounding
	int ascWidth = (int)(lenWidRatio * (double)(ascHeight) * (((double)src.cols) / ((double)src.rows)));
	int giantAscWidth = ascWidth * 2;
	int giantAscHeight = ascHeight * 2;

	// figure out how many pixels to each character -- use double width and double height.
	// Add to the pixel width to be sure all lines are seen, and then be careful not to read nonexistant pixels later
	//int pixHeight = (detected_edges.rows / ascHeight) + 1;
	//int pixWidth = (detected_edges.cols / ascWidth) + 1; 
	int pixHeight = (detected_edges.rows / giantAscHeight) + 1;
	int pixWidth = (detected_edges.cols / giantAscWidth) + 1;

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
			if (isWhite(x*pixWidth, (x + 1) * pixWidth, y*pixHeight, (y+1)*pixHeight)) {
				giantAsc[x + y * giantAscWidth] = '#';
			}
			else {
				giantAsc[x + y * giantAscWidth] = ' ';
			}
		}
		giantAsc[(y + 1) * giantAscWidth - 1] = '\n';
	}
	giantAsc[giantAscHeight * giantAscWidth - 1] = '\0';
	printf("%s\n", (char*)giantAsc);

	// now go through each section and condense into one char
	simpleReplace(ascHeight, ascWidth, ascArt, giantAsc);


	// free the ascii data
	free(giantAsc);
	free(ascArt);
}

/**************************************
 * Opencv wrappers ********************
 **************************************/
// wrapper functions for the opencv library functions to make things simpler

static void CannyThreshold(int, void*)
{
	if (blurThreshold == 0) blurThreshold = 1;
	blur(src_gray, detected_edges, Size(blurThreshold, blurThreshold));
	Canny(detected_edges, detected_edges, lowThreshold, lowThreshold * ratio, kernel_size);
	imshow(window_name, detected_edges);
	outlineToAscii();
}

/* demoImage: display an image and allow users to tweak the settings so they know what to specify later */
void demoImage(String fileName) {
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image!\n" << std::endl;
		std::cout << "Usage: " << "<THIS-FILE>" << " <Input image>" << std::endl;
		return;
	}
	//set up for processing
	cvtColor(src, src_gray, COLOR_BGR2GRAY);

	//show it off
	namedWindow(window_name, WINDOW_NORMAL);
	createTrackbar("Min Threshold:", window_name, &lowThreshold, max_lowThreshold, CannyThreshold);
	createTrackbar("Blur Threshold:", window_name, &blurThreshold, max_blurThreshold, CannyThreshold);
	createTrackbar("ratio:", window_name, &ratio, max_ratio, CannyThreshold);
	createTrackbar("size:", window_name, &ascHeight, maxAscHeight, CannyThreshold);
	CannyThreshold(0, 0);

	waitKey(0);
}

/**************************************
 * main function **********************
 **************************************/
// currently being used for testing. TODO: move to another file so this can more easily be used as a library

int main(int argc, char** argv)
{

	// for now, just focus on this
	if(argc <= 1){
		std::cout << "ERROR: please enter at least one image file to convert to ascii" << std::endl;
		return -1;
	}
	demoImage(argv[1]);
	return 0; 


	String outFile = "./"; // assume current dir unless told otherwise  
 //TODO: find a way to let this use the demo option istead (-d, perhaps? only one input file, and prints output)
	String ussage = "Ussage: <this-file>.exe [-o <OUTPUT-LOCATION>] <INPUT-FILE> [<INPUT-FILE> <INPUT-FILE> ...]\n";
	std::vector<String> inFiles;
	// parse the input
	if (argc == 0) {
		std::cout << "ERROR: please enter at least one image file to convert to ascii" << std::endl;
		std::cout << ussage;
		return -1;
	}
	for (int i = 0; i < argc; i++) {
		if (strcmp("-o", argv[i])) {
			// this is the output file. Check to make sure has a next arg
			if (i + 1 >= argc) {
				std::cout << "ERROR: if you specify -o, you MUST sepcify the output directory" << std::endl;
				std::cout << ussage;
				return -1;
			}
			outFile = argv[i];
		}
		else {
//			if (std::filesystem::is_directory(argv[i])) {
//				// if is a directory, add all of the files in it
//			}
//			else {
//				// everything else is a file name
//				inFiles.push_back(argv[i]);
//			}
		}
	}
	//get the image
	demoImage("C:\\Users\\l3go3\\OneDrive\\Pictures\\testImages\\smile.png");
	//CommandLineParser parser(argc, argv, "{@input | C:\\Users\\l3go3\\OneDrive\\Pictures\\testImages\\smile.png | input image}");
	
	return 0;
}