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
int demokernelSize;
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

// TODO: Try an actual machine learning option -- use either feature recognition, ascii recognition if it has it, or even train my own if I can
// TODO: a method that only looks for lines, just in a lot of detail (so handling up to like x^3 for ~ or something) could work well too. Perhaps HoughLinesP() -- returns a list of the lines found. So U would be like 3 lines?
// TODO: find a better way of using convolution to replace the (removed) region matching 
// TODO: implement a method that attempts to outline shapes based on the double sized input. 
// TODO: add a simple method that just draws the image with a single character

/* simpleReplace: scale the image down. Are just 16 possible items to translate (assumes 2x each dimension)
 * int ascHeight: the height of the final image in characters
 * int ascWidth: the width of the final image in characters 
 * char* result: the array to store the final result in
 * char* giant: the array containing the scaled up version of the ascii art image
 */
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

/* outlineToAscii: Divides the image up into regions to be handled by an ascii identification function. Prints out the final result 
 * Mat src: the image supplied by the user to be converted into ascii art
 * int ascHeight: the targe height of the ascii art in characters
*/
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
	memset(ascArt, '\0', ((int)ascHeight) * (ascWidth));
	
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

/* CannyThreshold: this is used for the demo to make it possible to have an interactive window.
 * params are used only by the library; simply pass 0,0 when calling. 
*/
static void CannyThreshold(int, void*)
{
	if (demoBlurThreshold == 0) demoBlurThreshold = 1;
	blur(demoSrcGray, demoDetectedEdges, Size(demoBlurThreshold, demoBlurThreshold));
	Canny(demoDetectedEdges, demoDetectedEdges, demoLowThreshold, demoLowThreshold * demoRatio, demokernelSize);
	imshow(WINDOW_NAME, demoDetectedEdges);
	//print the result
	char * result = outlineToAscii(demoDetectedEdges, demoAsciiHeight);
	std::cout << result << std::endl;
	free(result);
}

/* demoImage: display an image and allow users to tweak the settings so they know what to specify later 
 * String fileName: path to the image supplied by the user
 * int blurThreshold: parameter for image preprocessing
 * int lowThreshold: parameter for image preprocessing
 * int ratio: parameter for image preprocessing
 * int kernelSize: parameter for image preprocessing
 * int ascHeight: the target size for the final image in characters 
*/
void demoImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight) {
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
	demokernelSize = kernelSize;
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

/* convertImage: convert an image to ascii and return the result as a character array
 * String fileName: path to the image supplied by the user
 * int blurThreshold: parameter for image preprocessing
 * int lowThreshold: parameter for image preprocessing
 * int ratio: parameter for image preprocessing
 * int kernelSize: parameter for image preprocessing
 * int ascHeight: the target size for the final image in characters 
*/
char* convertImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight){
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
	Canny(detectedEdges, detectedEdges, lowThreshold, lowThreshold * ratio, kernelSize);
	return outlineToAscii(detectedEdges, ascHeight);
}


/* main function *************************************************************/
// currently being used for testing. TODO: move to another file so this can more easily be used as a library

int main(int argc, char** argv)
{

	// for now, just focus on this
	if(argc <= 1){
		std::cout << "ERROR: please enter at least one image file to convert to ascii" << std::endl;
		return -1;
	}
	
	// set defaults and let args change if needed
	bool isDemo = false;
	String fileName = argv[1];
	int blurThreshold = 3;
	int lowThreshold = 21;
	int ratio = 4;
	int kernelSize = 3; 
	int ascHeight = 20;

	// iterate through args and set values accordingly
	for(int i = 2 ; i < argc ; i++){
		if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
		help:
			// ignore everything else to print help message
			std::cout << "Usage: " << argv[0] << " filename [-d] [-b blurThreshold] [-l lowThreshold] [-r ratio] [-k kernelSize] [-h asciiHeight]" << std::endl;
			std::cout << "this program converts images into ascii art. The first argument MUST be the filename.";
			std::cout << " Order of other arguments does not matter. " << std::endl;
			std::cout << "	-d			Runs the program in demo mode" << std::endl;
			std::cout << "	-b, --blur		Sets the blur threshold value" << std::endl;
			std::cout << "	-l, --low		Sets the low threshold value" << std::endl;
			std::cout << "	-r, --ratio		Sets the ratio value" << std::endl;
			std::cout << "	-k, --kernel		Sets the kernel size value. Must be 3, 5, or 7" << std::endl;
			std::cout << "	-c, --height		Sets the character height of the generated ascii art" << std::endl;
			// TODO: detail everything as I add it... Just sets the default for demo, or actual for the normal.
			return 0;
		}
		else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--demo")){
			isDemo = true;
		}
		else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--blur")){
			blurThreshold = std::stoi(argv[++i]);
			if(blurThreshold < 1 || blurThreshold > MAX_BLUR_THRESHOLD) goto help;
		}
		else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--low")){
			lowThreshold = std::stoi(argv[++i]);
			if(lowThreshold < 1 || lowThreshold > MAX_LOW_THRESHOLD) goto help;
		}else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--ratio")){
			ratio = std::stoi(argv[++i]);
			if(ratio < 1 || ratio > MAX_RATIO) goto help;
		}else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--kernel")){
			kernelSize = std::stoi(argv[++i]);
			if(kernelSize != 3 && kernelSize != 5 && kernelSize != 7) goto help;
		}else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--Height")){
			ascHeight = std::stoi(argv[++i]);
			if(ascHeight < 1 || ascHeight > MAX_ASCII_HEIGHT) goto help;
		}
	}

	char * result;
	// determine if should demo or not
	if(isDemo) demoImage(fileName, blurThreshold, lowThreshold, ratio, kernelSize, ascHeight);
	else{
		result = convertImage(fileName, blurThreshold, lowThreshold, ratio, kernelSize, ascHeight);
		std::cout << result << std::endl;
		free(result);
	} 
 	return 0; 
}