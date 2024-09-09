#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <vector>
#include <utility> 
#include <filesystem>
#include <queue>
#include "GenerateAscii.hpp"
// #define DEBUG_MODE
using namespace cv;

/************************************************************************/
/* ASCII Art Generator							*/
/*									*/
/* Copyright (C) 2024 Noah Board					*/
/*									*/
/* This program is free software: you can redistribute it and/or modify	*/
/* it under the terms of the GNU General Public License as published by	*/
/* the Free Software Foundation, either version 3 of the License, or	*/
/* (at your option) any later version.					*/
/*									*/
/* This program is distributed in the hope that it will be useful, but	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of		*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU	*/
/* General Public License for more details.				*/
/*									*/
/* You should have received a copy of the GNU General Public License	*/
/* along with this program. If not, see					*/
/* <https://www.gnu.org/licenses/>.					*/
/*									*/
/* Author: Noah Board							*/
/* Creation: 2023							*/
/* Description: Implements the functionality for generating ascii art	*/
/************************************************************************/


// globals for demo
// Canny Edge Detection
int demoBlurThreshold;
Mat demoSrcGray;
Mat demoDetectedEdges;
int demoLowThreshold;
int demoRatio;
int demokernelSize;
int demoAsciiHeight;
// Difference of Gaussians 
int demoKernalSize1;
int demoKernalSize2;
int demoMedianBlurSize;
int demoPixelThreshold;


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

/* simpleReplace: scale the image down. Are just 16 possible items to translate (assumes 2x each dimension)
 * int ascHeight: the height of the final image in characters
 * int ascWidth: the width of the final image in characters 
 * char* result: the array to store the final result in
 * char* giant: the array containing the scaled up version of the ascii art image
 */
void simpleReplace(int ascHeight, int ascWidth, char* result, char* giant) {
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
				case '\x00':	// |  |
						// |  |
					result[x + y * ascWidth] = ' '; 
					break;
				case '\x01':	// |# |
						// |  |
					result[x + y * ascWidth] = '`';
					break;
				case '\x02':	// | #|
						// |  |
					result[x + y * ascWidth] = '\'';
					break;
				case '\x03':	// |##|
						// |  |
					result[x + y * ascWidth] = '-';
					break;
				case '\x04':	// |  |
						// |# |
					result[x + y * ascWidth] = '.';
					break;
				case '\x05':	// |# |
						// |# |
					result[x + y * ascWidth] = '|';
					break;
				case '\x06':	// | #|
						// |# |
					result[x + y * ascWidth] = '/';
					break;
				case '\x07':	// |##|
						// |# |
					result[x + y * ascWidth] = '/';
					break;
				case '\x08':	// |  |
						// | #|
					result[x + y * ascWidth] = '.';
					break;
				case '\x09':	// |# |
						// | #|
					result[x + y * ascWidth] = '\\';
					break;
				case '\x0A':	// | #|
						// | #|
					result[x + y * ascWidth] = '|';
					break;
				case '\x0B':	// |##|
						// | #|
					result[x + y * ascWidth] = '\\';
					break;
				case '\x0C':	// |  |
						// |##|
					result[x + y * ascWidth] = '_';
					break;
				case '\x0D':	// |# |
						// |##|
					result[x + y * ascWidth] = 'L';
					break;
				case '\x0E':	// | #|
						// |##|
					result[x + y * ascWidth] = '/';
					break;
				case '\x0F':	// |##|
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
char * outlineToAscii(Mat src, int ascHeight) {
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

// TODO: use the Sobel Filter to get angles on curves. 
// May want to add "difference of gausians" as a preprocess step

/**************************************
 * Opencv wrappers ********************
 **************************************/
// wrapper functions for the opencv library functions to make things simpler

/* CannyThreshold: this is used for the demo to make it possible to have an interactive window.
 * params are used only by the library; simply pass 0,0 when calling. 
*/
void CannyThreshold(int, void*)
{
	if(demoAsciiHeight < 1 ) demoAsciiHeight = 1;
	if (demoBlurThreshold < 1) demoBlurThreshold = 1;
	blur(demoSrcGray, demoDetectedEdges, Size(demoBlurThreshold, demoBlurThreshold));
	Canny(demoDetectedEdges, demoDetectedEdges, demoLowThreshold, demoLowThreshold * demoRatio, demokernelSize);
	imshow(WINDOW_NAME_C, demoDetectedEdges);
	//print the result
	char * result = outlineToAscii(demoDetectedEdges, demoAsciiHeight);
	std::cout << result << std::endl;
	free(result);
}


/* diffOfGaussians: this is used for the demo to make it possible to have an interactive window.
 * params are used only by the library; simply pass 0,0 when calling. 
*/
void diffOfGaussians(int, void*){
	Mat gaus1, gaus2, medBlur;

	// copy image so can make changes without affecting original
	demoDetectedEdges = demoSrcGray.clone();

	// correct input values
	if(!(demoKernalSize1&1)) demoKernalSize1+=1;
	if(!(demoKernalSize2&1)) demoKernalSize2+=1;
	if(!(demoMedianBlurSize&1)) demoMedianBlurSize+=1;
	if(!(demoPixelThreshold&1)) demoPixelThreshold+=1;

	// blur first to help
	medianBlur(demoDetectedEdges, medBlur, demoMedianBlurSize);

	// perform edge detection
	GaussianBlur(medBlur, gaus1, Size(demoKernalSize1,demoKernalSize1), 0);
	GaussianBlur(medBlur, gaus2, Size(demoKernalSize2,demoKernalSize2), 0);
	demoDetectedEdges = gaus1 - gaus2;

	//now brighten everything past the threshold and delete the rest
	Mat mask;
	inRange(demoDetectedEdges, Scalar(demoPixelThreshold, demoPixelThreshold, demoPixelThreshold), 
		Scalar(255, 255, 255), mask);
	demoDetectedEdges.setTo(Scalar(255, 255, 255), mask);
	inRange(demoDetectedEdges, Scalar(0, 0, 0), 
		Scalar(demoPixelThreshold, demoPixelThreshold, demoPixelThreshold), mask);
	demoDetectedEdges.setTo(Scalar(0, 0, 0), mask);

	// update demo window
	imshow(WINDOW_NAME_G, demoDetectedEdges);

	// print the result
	char * result = outlineToAscii(demoDetectedEdges, demoAsciiHeight);
	std::cout << result << std::endl;
	free(result);
	}

/** demoCannyImage: display an image and allow users to tweak the settings for canny edge detection 
 * so they know what to specify later 
 * fileName:		path to the image supplied by the user
 * blurThreshold:	parameter for image preprocessing
 * lowThreshold:	parameter for image preprocessing
 * ratio:		parameter for image preprocessing
 * kernelSize:		parameter for image preprocessing
 * ascHeight:		the target size for the final image in characters 
**/
void demoCannyImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight) {
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
	demoSrcGray = srcGray;
	demoBlurThreshold = blurThreshold;
	demoDetectedEdges = detectedEdges;
	demoLowThreshold = lowThreshold;
	demoRatio = ratio;
	demokernelSize = kernelSize;
	demoAsciiHeight = ascHeight;

	//show it off
	namedWindow(WINDOW_NAME_C, WINDOW_NORMAL);
	createTrackbar("Min Threshold:", WINDOW_NAME_C, &demoLowThreshold, MAX_LOW_THRESHOLD, CannyThreshold);
	createTrackbar("Blur Threshold:", WINDOW_NAME_C, &demoBlurThreshold, MAX_BLUR_THRESHOLD, CannyThreshold);
	createTrackbar("ratio:", WINDOW_NAME_C, &demoRatio, MAX_RATIO, CannyThreshold);
	createTrackbar("size:", WINDOW_NAME_C, &demoAsciiHeight, MAX_ASCII_HEIGHT, CannyThreshold);
	CannyThreshold(0, 0);

	waitKey(0);
}

/** demoCannyImage: display an image and allow users to tweak the settings for canny edge detection 
 * so they know what to specify later 
 * fileName:		path to the image supplied by the user
 * blurThreshold:	parameter for image preprocessing
 * lowThreshold:	parameter for image preprocessing
 * ratio:		parameter for image preprocessing
 * kernelSize:		parameter for image preprocessing
 * ascHeight:		the target size for the final image in characters 
**/
void demoGaussImage(String fileName, int kernalSize1, int kernalSize2, int medianBlurSize, int pixelThreshold, int ascHeight){
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
	demoSrcGray = srcGray;
	demoKernalSize1 = kernalSize1;
	demoKernalSize2 = kernalSize2;
	demoDetectedEdges = detectedEdges;
	demoMedianBlurSize = medianBlurSize;
	demoPixelThreshold = pixelThreshold;
	demoAsciiHeight = ascHeight;
	
	// show off gaussian 
	namedWindow(WINDOW_NAME_G, WINDOW_NORMAL);
	createTrackbar("Median Blur:", WINDOW_NAME_G, &demoMedianBlurSize, MAX_MEDIAN_BLUR_SIZE, diffOfGaussians);
	createTrackbar("Gauss 1:", WINDOW_NAME_G, &demoKernalSize1, MAX_KERNAL_SIZE_1, diffOfGaussians);
	createTrackbar("Gauss 2:", WINDOW_NAME_G, &demoKernalSize2, MAX_KERNAL_SIZE_2, diffOfGaussians);
	createTrackbar("Brightness Threshold:", WINDOW_NAME_G, &demoPixelThreshold, MAX_PIXEL_THRESHOLD, diffOfGaussians);
	createTrackbar("size:", WINDOW_NAME_G, &demoAsciiHeight, MAX_ASCII_HEIGHT, diffOfGaussians);
	diffOfGaussians(0, 0);

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