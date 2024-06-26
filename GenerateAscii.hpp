#pragma once
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
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