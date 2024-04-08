#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include <iostream>
#include <vector>
#include "GenerateAscii.hpp"
// #define DEBUG_MODE
using namespace cv;

/************************************************************************/
/* ASCII Art Generator command line interface				*/
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

/* main function */
int main(int argc, char** argv)
{

	// for now, just focus on this
	if(argc <= 1){
		std::cout << "ERROR: please enter at least one image file to convert to ascii" << std::endl;
		return -1;
	}
	
	// set defaults and let args change if needed
	bool isDemo = false;
	String fileName;
	int blurThreshold = 3;
	int lowThreshold = 21;
	int ratio = 4;
	int kernelSize = 3; 
	int ascHeight = 20;

	// iterate through args and set values accordingly
	for(int i = 1 ; i < argc ; i++){
		if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
		help:
			// ignore everything else to print help message
			std::cout << "Usage: " << argv[0] << " filename [-d] [-b blurThreshold] [-l lowThreshold] [-r ratio] [-k kernelSize] [-h asciiHeight]" << std::endl;
			std::cout << "this program converts images into ascii art. The first argument MUST be the filename.";
			std::cout << " Order of other arguments does not matter. " << std::endl;
			std::cout << "	-h, --help		Prints this message. Ignores all other args." << std::endl;
			std::cout << "	-d, --demo		Runs the program in demo mode" << std::endl;
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
		}else{
			// just assume it was the file name
			fileName = argv[i];
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