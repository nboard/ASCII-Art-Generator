#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <vector>
#include <utility> 
#include <filesystem>
#include <queue>
#include <cmath>
#include "GenerateAscii.hpp"
// #define DEBUG_MODE
using namespace cv;

/************************************************************************/
/* ASCII Art Generator							*/
/*									*/
/* Copyright (C) 2025 Noah Board					*/
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


/* singleLinePhase: Calculates the angle from the X and Y components of the sobel filter. 
 *    This matches the functionality of the phase function from open CV, except that a single 
 *    line is produced instead of a double. 
 *    The function returns a Mat containing the results. All angles are between 0 and 360 
 *    (0 and 2pi). It contains -1 where there was a black (value <1) pixel. 
 * args:
 *	Mat xSobel: the x component of the sobel filter
 *	Mat ySobel: the y component of the sobel filter
 *	bool isDegrees: Controls for degrees or radians
*/ 
Mat singleLinePhase(Mat xSobel, Mat ySobel, bool isDegrees = true) {
	Mat angle;
	xSobel.copyTo(angle);
	for(int i = 0 ; i < angle.cols ; i++){
		for(int j = 0 ; j < angle.rows ; j++){
			if(ySobel.at<float>(j, i) > 1 || xSobel.at<float>(j, i) > 1){
				float convert = (isDegrees)?(180.0/3.14159):1.0;
				float temp = convert*(atan2(ySobel.at<float>(j, i), xSobel.at<float>(j, i)));
				angle.at<float>(j, i) = (temp < 0)? ((isDegrees)?360:2*3.14159)-temp:temp;
			} else {
				angle.at<float>(j, i) = -1;
			}
			//std::cout << angle.at<float>(j, i) << ", ";
		}
		//std::cout << std::endl;
	}
	return angle;
}

/* averageAngle: Calculates the average angle in radians between 0 and pi, inside a region of an angle vector.
 * The function ignores any angle marked as -1
 * This function treats equiilent lines as the same (say, 90 and 270), so the returned value
 * 	is always in the range [0, pi)
 * args:
 *	Mat angle: an image madeup of the angles calculated from the outline (assumes float)
 *	int xMin: minimum x pixel coordinate to check
 *	int xMax: maximum x pixel coordinate to check
 *	int yMin: minimum y pixel coordinate to check
 *	int yMax: maximum y pixel coordinate to check
*/ 
float averageAngle(Mat angle, int xMin, int xMax, int yMin, int yMax) {
	// find average of angles. Check bounds first

	if (xMin >= angle.cols) return -1;
	if (yMin >= angle.rows) return -1;
	if (xMax > angle.cols) xMax = angle.cols;
	if (yMax > angle.rows) yMax = angle.rows;

	// convert angles to vectors, add all of the vectors, and find angle of result


	double vectorX = 0;
	double vectorY = 0;
	int cnt = 0;
	for (int x = xMin; x < xMax; x++) {
		for (int y = yMin; y < yMax; y++) {
			if(angle.at<float>(y, x) >= 0){
				double raw = (double) angle.at<float>(y, x);
				// need to treat angles that creat the same line as equivilent, so force y to be positive
				double coordX  = cos(raw);
				double coordY = abs(sin(raw));
				if(coordY < 0) {
					vectorX -= coordX;
					vectorY -= coordY;
				} else {
					vectorX += coordX;
					vectorY += coordY;
				}
				cnt++;
			}
		}
	}
	float total = (xMax - xMin + 1)*(yMax - yMin + 1);
	// skip blank or mostly blank areas
	if(cnt < (xMax - xMin) || cnt < (yMin - yMax) || (vectorX < 0.001 && vectorY < 0.001)) return -1; //|| (cnt/total) < ((xMax - xMin)/total)) return -1;
	double ret = atan2(vectorY, vectorX);
	float temp = (float) ret;
	return ret;
}

/**************************************
 * Ascii Identification ***************
 **************************************/
// These functions convert the pixels in a region into one ascii character

/* simpleReplace: scale the image down. There are just 16 possible items to translate 
 * NOTE: assumes 2x each dimension for the source array
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

/* angleReplace: scale the image down based on combinations of lines. 
 * NOTE: assumes 2x each dimension for the source array, and angles in radians between 0 and pi
 * int ascHeight: the height of the final image in characters
 * int ascWidth: the width of the final image in characters 
 * char* result: the array to store the final result in
 * char* source: the array containing the scaled up version of the ascii art image
 */
void angleReplace(int ascHeight, int ascWidth, char* result, float *source) {
	
	// Define directions in radians. Defined like compas directions
	const float RAD_N = M_PI/2;
	const float RAD_NE = M_PI/3;
	const float RAD_EN = M_PI/6;
	const float RAD_E = 0;
	// no south, since angles are between 0 and pi
	const float RAD_W = M_PI;
	const float RAD_WN = 5*M_PI/6;
	const float RAD_NW = 3*M_PI/4;
	
	int dblWidth = ascWidth * 2 - 1;
	int dblHeight = ascHeight * 2;
	for (int y = 0; y < ascHeight; y++) {
		for (int x = 0; x < ascWidth - 1; x++) {
			// get the four values:	|a1|b1|
			//			|a2|b2|
			int square = 0;
			int a1 =  source[(2 * x)	+ (2 * y)	* dblWidth];
			int b1 =  source[(2 * x + 1)	+ (2 * y)	* dblWidth];
			int a2 =  source[(2 * x)	+ (2 * y + 1)	* dblWidth];
			int b2 =  source[(2 * x + 1)	+ (2 * y + 1)	* dblWidth];

			double avgX = 0;
			double avgY = 0;
			double avg = -1; 
			int cnt = 0;
			// use vectors to compute averages
			if(a1 >= 0){ 
				double tempX = cos((double)a1);
				double tempY = sin((double)a1);
				if(tempY < 0){
					avgX -= tempX;
					avgY -= tempY;
				}
				else{
					avgX += tempX;
					avgY += tempY;
				}
				cnt++; 
			}
			if(b1 >= 0){ 
				double tempX = cos((double)b1);
				double tempY = sin((double)b1);
				if(tempY < 0){
					avgX -= tempX;
					avgY -= tempY;
				}
				else{
					avgX += tempX;
					avgY += tempY;
				}
				cnt++; 
			}
			if(a2 >= 0){ 
				double tempX = cos((double)a2);
				double tempY = sin((double)a2);
				if(tempY < 0){
					avgX -= tempX;
					avgY -= tempY;
				}
				else{
					avgX += tempX;
					avgY += tempY;
				}
				cnt++; 
			}
			if(b2 >= 0){ 
				double tempX = cos((double)b2);
				double tempY = sin((double)b2);
				if(tempY < 0){
					avgX -= tempX;
					avgY -= tempY;
				}
				else{
					avgX += tempX;
					avgY += tempY;
				}
				cnt++; 
			}
			if(cnt > 0) avg = atan2(avgX, avgY);
			if(avg < 0) avg += 2*M_PI;
		/*   */ if(cnt == 0)
				result[x + y * ascWidth] = ' '; // shortcut a common case
		/* L */ else if((a1 < RAD_NW && a1 > RAD_NE) 		&& b1 < 0		&& (a2 < RAD_NW && a2 > RAD_NE) 	&& (b2 > RAD_WN || b2 < RAD_EN) && b2 > 0 )
				result[x + y * ascWidth] = 'L';
		/* _ */ else if( a1 < 0					&& b1 < 0		&& a2 > 0				&& b2 > 0		) 
				result[x + y * ascWidth] = '_';

			// if all else fails, just use average angle
			else if(avg >= RAD_NE && avg <= RAD_NW) result[x + y * ascWidth] = '|';
			else if(avg <= RAD_EN || avg >= RAD_WN) result[x + y * ascWidth] = '-';
			else if(avg > RAD_EN && avg < RAD_NE) result[x + y * ascWidth] = '/';
			else if(avg <= RAD_WN && avg > RAD_NW) result[x + y * ascWidth] = '\\';
			else result[x + y * ascWidth] = '?';

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
// these functions use an ascii identification function convert a preprocessed image into ascii art 

/* outlineToAscii: Divides the image up into regions to be handled by an ascii identification function. Prints out the final result 
 * Mat src:		the image supplied by the user to be converted into ascii art
 * int ascHeight:	the height of the ascii art in characters
*/
char * outlineToAscii(Mat src, int ascHeight) {
	// Create the grid for the art. Start with heigh and calculate the width
	// TODO: look into how much this warps the image by rounding
	int ascWidth = (int)(LEN_WID_RATIO * (double)(ascHeight) * (((double)src.cols) / ((double)src.rows)));
	int giantAscWidth = ascWidth * 2;
	int giantAscHeight = ascHeight * 2;

	// figure out how many pixels to each character -- use double width and double height.
	// Add to the pixel width to be sure all lines are seen, and then be careful not to read nonexistant pixels later
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

/* sobelToAscii: Uses the Sobel filter to transform an image into the angles of the outlines. 
 *	These are then transformed into ascii art.
 * Mat src:		the image supplied by the user to be converted into ascii art
 * int ascHeight:	the height of the ascii art in characters
*/
char * sobelToAscii(Mat src, int ascHeight) {
// Create the grid for the art. Start with heigh and calculate the width
	// TODO: look into how much this warps the image by rounding
	int ascWidth = (int)(LEN_WID_RATIO * (double)(ascHeight) * (((double)src.cols) / ((double)src.rows)));
	int dblWidth = ascWidth * 2;
	int dblHeight = ascHeight * 2;

	// figure out how many pixels to each character
	// Add to the pixel width to be sure all lines are seen, and then be careful not to read nonexistant pixels later
	int pixHeight = (src.rows / dblHeight) + 1;
	int pixWidth = (src.cols / dblWidth) + 1;

	// Now make a grid of those dimensions
	ascWidth++; // add an extra for the end lines
	dblWidth++;
	char* ascArt = (char*)malloc(sizeof(char) * ascHeight * ascWidth);
	float* dblArt = (float*)malloc(sizeof(float) * dblHeight * dblWidth);
	memset(ascArt, '\0', ((int)ascHeight) * (ascWidth));
	memset(dblArt, '\0', (sizeof(float) * dblHeight * dblWidth));
	
	// need to run spatialGradient() to get x and y. Then for each pixel, run arctan. Then average angles for each region, then angle -> asciii
	// later, want to do maybe quarter regions to help spot ^v<>, and maybe even ()UnO

	Mat xSobel, ySobel, angle;
	//src.convertTo(src, CV_32FC1);
	Sobel(src, xSobel, 5, 1, 0, 1);
	Sobel(src, ySobel, 5, 0, 1, 1);
	//phase(xSobel, ySobel, angle, true);
	angle = singleLinePhase(xSobel, ySobel, false);
	// This should probably be a double line on snoopy - otherwise silhouettes would never showup right
printf("\n\n--------------------------------------------------------------------------\n");
	for (int y = 0; y < dblHeight; y++) {
		for (int x = 0; x < dblWidth - 1 ; x++) {
			dblArt[x + y * dblWidth] = averageAngle(angle, x*pixWidth, (x + 1) * pixWidth, y*pixHeight, (y+1)*pixHeight);
			printf("%f, ", dblArt[x + y * dblWidth]);
		}
		dblArt[(y + 1) * dblWidth - 1] = '\n';
		printf("\n");
	}
	printf("\n\n--------------------------------------------------------------------------\n");
//	printf("##########################################################################\n");
//	printf("--------------------------------------------------------------------------\n\n\n");
	//std::cout << angle << std::endl;
	dblArt[dblHeight * dblWidth - 1] = '\0'; // this replaces the last endline with an eof
	#ifdef DEBUG_MODE
		printf("%s\n", (char*)ascArt);
	#endif

	angleReplace(ascHeight, ascWidth, ascArt, dblArt);

	std::cout << dblArt << std::endl;


	return ascArt;
}

// TODO: make a line follow algorithm that just tries to link up adjacent "lit" areas

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
	Mat xSobel, ySobel, angle;
	//src.convertTo(src, CV_32FC1);
	Sobel(demoDetectedEdges, xSobel, 5, 1, 0, 1);
	Sobel(demoDetectedEdges, ySobel, 5, 0, 1, 1);
	angle = singleLinePhase(xSobel, ySobel);
	imshow(WINDOW_NAME_G, angle);

	// print the result
	char * result = sobelToAscii(demoDetectedEdges, demoAsciiHeight); //outlineToAscii(demoDetectedEdges, demoAsciiHeight);
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

/** demoGaussImage: display an image and allow users to tweak the settings for gauss edge detection 
 * so they know what to specify later 
 * String fileName:	path to the image supplied by the user
 * int kernalSize1:	initial Kernal size for the first gaussian blur
 * int kernalSize2:	initial Kernal size for the second gaussian blur
 * int medianBlurSize:	parameter for image preprocessing
 * int pixelThreshold:	brightness threshold for post processed pixels to be considered
 * int ascHeight:	the target size for the final image in characters 
**/
void demoGaussImage(String fileName, int kernalSize1, int kernalSize2, int medianBlurSize, int pixelThreshold, int ascHeight){
	Mat src, srcGray, detectedEdges;
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image!\n" << std::endl;
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


/* convertCannyImage: convert an image to ascii and return the result as a character array
 * Uses canny edge detection and converts lines to ascii characters based on the presence 
 * or absence of pixels.
 * String fileName:	path to the image supplied by the user
 * int blurThreshold:	parameter for image preprocessing
 * int lowThreshold:	parameter for image preprocessing
 * int ratio:		parameter for image preprocessing
 * int kernelSize:	parameter for image preprocessing
 * int ascHeight:	the target size for the final image in characters 
*/
char* convertCannyImage(String fileName, int blurThreshold, int lowThreshold, int ratio, int kernelSize, int ascHeight){
	Mat src, srcGray, detectedEdges;
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image " << fileName << std::endl;
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

/* convertGaussImage: convert an image to ascii and return the result as a character array
 * Uses a difference of gaussians and sobel, and uses the resulting angels to create ascii
 * String fileName:	path to the image supplied by the user
 * int kernalSize1:	initial Kernal size for the first gaussian blur
 * int kernalSize2:	initial Kernal size for the second gaussian blur
 * int medianBlurSize:	parameter for image preprocessing
 * int pixelThreshold:	brightness threshold for post processed pixels to be considered
 * int ascHeight:	the target size for the final image in characters 
**/
char* convertGaussImage(String fileName, int kernalSize1, int kernalSize2, int medianBlurSize, int pixelThreshold, int ascHeight){
	Mat src, srcGray, detectedEdges, gaus1, gaus2, medBlur;
	
	src = imread(samples::findFile(fileName), IMREAD_COLOR); // Load an image
	if (src.empty())
	{
		std::cout << "Could not open or find the image!\n" << std::endl;
		return NULL;
	}

	//set up for processing
	cvtColor(src, srcGray, COLOR_BGR2GRAY);
	
	// copy image so can make changes without affecting original
	detectedEdges = srcGray.clone();

	// correct input values
	if(!(kernalSize1&1)) kernalSize1+=1;
	if(!(kernalSize2&1)) kernalSize2+=1;
	if(!(medianBlurSize&1)) medianBlurSize+=1;
	if(!(pixelThreshold&1)) pixelThreshold+=1;

	// blur first to help
	medianBlur(detectedEdges, medBlur, medianBlurSize);

	// perform edge detection
	GaussianBlur(medBlur, gaus1, Size(kernalSize1,kernalSize1), 0);
	GaussianBlur(medBlur, gaus2, Size(kernalSize2,kernalSize2), 0);
	detectedEdges = gaus1 - gaus2;

	//now brighten everything past the threshold and delete the rest
	Mat mask;
	inRange(detectedEdges, Scalar(pixelThreshold, pixelThreshold, pixelThreshold), 
		Scalar(255, 255, 255), mask);
	detectedEdges.setTo(Scalar(255, 255, 255), mask);
	inRange(detectedEdges, Scalar(0, 0, 0), 
		Scalar(pixelThreshold, pixelThreshold, pixelThreshold), mask);
	detectedEdges.setTo(Scalar(0, 0, 0), mask);

	// print the result
	return sobelToAscii(detectedEdges, ascHeight);
}