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

// always returns the # character. Good to get rough outlines of images. Only good for huge images
char constChar(int xMin, int xMax, int yMin, int yMax) {
	return '#';
}

// given an image and the subset of pixels to look at, determine which ascii character is the closest
// this method attempts to determine which character to use based on the region that are colored in the image and matching that to a known image
// is flawed in that it does not allow the same shape to be shifted and get any kind of similar result. Probably better for larger images, where course details work well...
char regionChar(int xMin, int xMax, int yMin, int yMax) {
	// note: x = columns = i, y = rows = j
	// use six regions: four corners, center, and edge
	float topL = 0, botL = 0, topR = 0, botR = 0;
	float center = 0, edge = 0, total = 0;
	float horizontal = 0, vertical = 0;
	int xSize = (xMax - xMin + 1);
	int xBorder = (xSize / 2) + xMin;
	int xCenterL = (xSize / 3) + xMin;
	int xCenterR = (xSize * 2 / 3) + xMin;
	int ySize = (yMax - yMin + 1);
	int yBorder = (ySize / 2) + yMin;
	int yCenterT = (ySize / 3) + yMin;
	int yCenterB = (ySize * 2 / 3) + yMin;
	
	printf("\n--%d-%d--start--%d-%d----\n", yMin, yMax, xMin, xMax);
	for (int j = yMin; j <= yMax; j++) {
		for (int i = xMin; i <= xMax; i++) {
			if (detected_edges.at<char>(j, i) != 0) {
				// found a white pixel. inc the relivant regions.
				// Note that if there is an odd dimension, right and bottom get the extra pixel
				if (i < xBorder && j < yBorder) topL++;
				else if (i < xBorder && j >= yBorder) botL++;
				else if (i >= xBorder && j < yBorder) topR++;
				else if (i >= xBorder && j >= yBorder) botR++;

				// center region is considerably smaller in area than the edge. It just needs to tell O from * for the most part
				if (i >= xCenterL && i <= xCenterR && j >= yCenterT && j <= yCenterB) center++;
				else edge++;

				// horizontal and vertical are f or finding -, |, and +
				if (i >= xCenterL && i <= xCenterR) vertical++;
				if (j >= yCenterT && j <= yCenterB) horizontal++;

				total++;
				printf("##");
			}
			else printf("  ");
		}
		printf("\n");
	}
	
	// scale results to % of the pixels found
	if (total < 1) return ' ';
	topL /= total;
	topR /= total;
	botL /= total;
	botR /= total;
	center /= total;
	edge /= total;
	horizontal /= total;
	vertical /= total;
	float area = xSize * ySize;
	float density = total / area;
	printf("top left:     %f\ntop right:    %f\nbottom left:  %f\nbottom right: %f\ncenter:       %f\nedge:         %f\ndensity:      %f\nhorizontal:   %f\nvertical:     %f\n", 
		topL, topR, botL, botR, center, edge, density, horizontal, vertical);
	printf("-------end--------\n");
	// now pick the which character desired
	if (topL > .25 && topR < 0.1 && botL < 0.1 && botR > 0.5) return '\\';
	else if (topL < .1 && topR > .25 && botL > 0.25 && botR < 0.1) return '/';
	else if (center > .9 && topL < .35 && topR < .35 && botL < .35 && botR < .35) return '*';
	else if (center < .4 && topL < .35 && topR < .35 && botL < .35 && botR < .35) return 'O';
	else if (horizontal > .6 && vertical > .6 && center < .5) return '+';
	else if (horizontal < .5 && vertical > .8) return '|'; // top and bottom symitry, not left to right
	else if (horizontal > .8 && vertical < .5) return '-';
	else if (edge > .5 && botL + botR > .8) return '_';
	else if (density > 0.5) return '#';
	return '?';
}

// determine what the best character is based off of some line of best fit type math... (Actually, it does some primative feature matches, and then gives up and does line of best fit)
// tries to determine +=-_()[]\/|<>.':`~#^*0oXUV
// Ultimately, follows the hopeless task of trying to shrink enless kinds of possible combos into a finite set. Bad idea
// TODO: try some extra level of convilution so always falls into one of the desired slots. Clump together more...?
	
char lineChar(int xMin, int xMax, int yMin, int yMax) {
	// so maybe we just kinda itterate over Y and pick out the faeture that on X... either 1 spot, 2 sopts, or line accross. 
	// if 2 spots for more than 1 y value, then can tell if converge or diverge based on if was every 1 point at a point. 
	// if a single point 
	// note: x = columns = i, y = rows = j
	// use six regions: four corners, center, and edge
	float total = 0, totalX = 0, totalY = 0, xCount = 0;
	float avgX, avgY;
	int xSize = (xMax - xMin + 1);
	int ySize = (yMax - yMin + 1);
	int firstX = -1;
	int lastX = -1;
	int numFlips = 0; 
	float xAvgWid = 0;
	std::vector<int> xFeatures; //0 is blank, 1 is one point, 2 is two points, 3+ is a line
	std::vector<float> xCenters; // if I wind up needing to store pixel locations. 
	int prevX = -1;
	int xParts = 0;
#ifdef DEBUG_MODE
	printf("\n--%d-%d--start--%d-%d----\n", yMin, yMax, xMin, xMax);

	for (int j = yMin; j <= yMax; j++) {
		for (int i = xMin; i <= xMax; i++) {
			if (detected_edges.at<char>(j, i) != 0) {
				printf("##");
			}
			else printf("  ");
		}
		printf("\n");
	}
#endif
	// walk along and see what types of features have; look at slope compared to prev, etc
	for (int j = yMin; j <= yMax; j++) {
		for (int i = xMin; i <= xMax; i++) {
			if (detected_edges.at<char>(j, i) != 0) {
				if (firstX == -1) firstX = i;
				lastX = i;
				if (prevX != -1) {
					if (((float)(i - prevX)) / ((float)xSize) > 0.4) { //needs to be spread out to recognize a new slot
						xParts++;
					}
					prevX = i;
				}
				else {
					prevX = i;
					xParts++;
				}
				total++;
				totalX += i-xMin;
				totalY += j-yMin;
			//	printf("##");
			}
			//else printf("  ");
		}
		if (xParts > 3) xParts = 3;
		// detect if was a continous line.   
		else if (xParts == 1 && ((float)(lastX - firstX) / ((float)xSize) > 0.5) && firstX != lastX) xParts = 3;
		// if the count is new, then add in the next feature
		if (xFeatures.empty() || xFeatures.back() != xParts) xFeatures.push_back(xParts);
		
		// only update x width average if something was found
		if (xParts != 0) {
			xAvgWid += ((float)(lastX - firstX)) / ((float)xSize);
			xCount++;

			// also get x center
			xCenters.push_back(((float)(lastX + firstX)) / 2);
		}

		prevX = -1;
		firstX = -1;
		xParts = 0;
		//printf("\n");
	}

	// take the average pos, and scale it to %
	avgX = (totalX / (float)total) / (float)(xSize);	// average x %
	avgY = (totalY / (float)total) / (float)(ySize);	// average y %
	float density = total / (float)((xSize) * (ySize));	// density
	xAvgWid /= (float)xCount;							// average of the x widths wherever lines were 
	// print out what it looks like to the program
#ifdef DEBUG_MODE
	for (int i = 0; i < xFeatures.size(); i++) {
		if (xFeatures.at(i) == 3) printf("horiz line\n");
		else if (xFeatures.at(i) == 2) printf("two chunks\n");
		else if (xFeatures.at(i) == 1) printf("one chunk\n");
		else if (xFeatures.at(i) == 0) printf("nuthin'\n");
		else printf("err... something?\n");
	}
	printf("avg x:   %f\navg y:   %f\ndensity: %f\nx width: %f\n", avgX, avgY, density, xAvgWid);
	printf("-------end--------\n");
#endif

	// sooo... I guess let's look at the features. FIXME: not considering  ~*
	// ignore if is blank at start or end
	if (!xFeatures.empty() && xFeatures.at(0) == 0) xFeatures.erase(xFeatures.begin());
	if (!xFeatures.empty() && xFeatures.back() == 0) xFeatures.pop_back();
	// if it is empty, just return a space 
	if (xFeatures.empty()) return ' ';

	// 1 --> |[]() \/ -_ <> 
		// can do line of best fit, or just general tracking on how it moved from line to line -- left, right, or stay
		// a high x width indicates a horixontal line (so maybe .3 and up)
	if (xFeatures.size() == 1 && xFeatures.at(0) == 1) {

		// if very low density but low avg y --> '
		if (density < 0.1 && avgY < 0.1) return '\'';
		// low density and high avg y --> .
		if (density < 0.1 && avgY > 0.9) return '.';
		//if high width or very few parts --> -_ (Snuck in lol)
		if (xAvgWid > 0.3 || ((float)xCenters.size())/((float)ySize) < 0.1) {
			if (avgY < 0.5) return '-';
			else return '_';
		}

		// look through points and find line
		int changes = 0;
		int prevX = xCenters.front();
		int curX;
		int posParts, negParts, zerParts;
		int prevSign = 0, curSign = 0;
		int firstHalfDir = 0;
		int secondHalfDir = 0;
		posParts = negParts = zerParts = 0;
		// NOTE: if this doesn't work well, consider just adding the raw change rather than the sign so large movements count for more. 
		for (int k = 1; k < xCenters.size(); k++) {
			curX = xCenters.at(k);
			int dif = curX - prevX; //keeps the signs matching with positive (/) and negative (\) lines
			if (dif > xSize/10) {
				posParts++;
				curSign = 1;
			}
			else if (dif < -xSize/10) {
				negParts++;
				curSign = -1;
			}
			else {
				zerParts++;
				curSign = 0;
			}

			if (k < xCenters.size() / 2) firstHalfDir += curSign;
			else secondHalfDir += curSign;

			if (prevSign != curSign) changes++;
			prevX = curX;
			prevSign = curSign;
		}
		float percPosParts = ((float)posParts) / ((float)xCenters.size()-1);
		float percNegParts = ((float)negParts) / ((float)xCenters.size()-1);
		float percZerParts = ((float)zerParts) / ((float)xCenters.size()-1);
#ifdef DEBUG_MODE
		printf("pos:    %f\nneg:    %f\nzero:  %f\n", percPosParts, percNegParts, percZerParts);
#endif
		if (changes == 0 || percZerParts > 0.8) return '|'; // stayed |
		if (percPosParts > 0.7) return '\\';
		if (percNegParts > 0.7) return '/';
		if (firstHalfDir > 0 && secondHalfDir < 0) return '>';
		if (firstHalfDir < 0 && secondHalfDir > 0) return '<';

		// TODO: if we end up considering magnitude, maybe try tracking of sharp, big changes or gentle.
		// curved higher lower --> )
		// curved lower higher --> (
	
		// if very low avg x --> [
		//if (avgX < 0.2) return '[';
		//if (avgX > 0.8) return ']';
		// if all else fails, just do the normal line
		return '|';
	}
	// some mix of horiz and one chunks, and is wide --> -_   (maybe ~ belongs here)
	if (xAvgWid > 0.3 && (xFeatures.at(0) == 3 || xFeatures.at(0) == 1) && (xFeatures.size() < 2 || (xFeatures.at(1) == 3 || xFeatures.at(1) == 1)) && 
		(xFeatures.size() < 3 || (xFeatures.at(2) == 3 || xFeatures.at(2) == 1)) ) {
		//if high width --> -_... Snuck in lol
		if (avgY < 0.5) return '-';
			else return '_';
	}
	// 131 --> +
	if (xFeatures.size() == 3 && xFeatures.at(0) == 1 && xFeatures.at(1) == 3 && xFeatures.at(2) == 1) return '+';
	// 101 --> = or :
	if (xFeatures.size() == 3 && xFeatures.at(0) == 1 && xFeatures.at(2) == 0 && xFeatures.at(2) == 1) {
		if (xAvgWid > 0.3) return '=';
		else return ':';
	}
	// 121 --> 0 or o, depends on avg y
	if (xFeatures.size() == 3 && xFeatures.at(0) == 1 && xFeatures.at(2) == 2 && xFeatures.at(2) == 1) {
		if (avgY > 0.5) return 'o';
		else return 'O';
	}
	// 212 --> X
	if (xFeatures.size() == 3 && xFeatures.at(0) == 2 && xFeatures.at(1) == 1 && xFeatures.at(2) == 2) return '+';
	//21 --> U or V (depends on sharpness/thickness)
	if (xFeatures.size() == 2 && xFeatures.at(0) == 2 && xFeatures.at(1) == 1) {
		if (xAvgWid > 0.1) return 'U';
		else return 'V';
	}
	//12 --> ^
	if (xFeatures.size() == 2 && xFeatures.at(0) == 1 && xFeatures.at(1) == 2) return '^';
	// if junk but dense, #
	if(density > 0.5) return '#';
	// who knows... find a char that always looks good later lol
	return '?';
}

// TODO: Try an actual machine learning option -- use either feature recognition, ascii recognition if it has it, or even train my own if I can
// TODO: a method that only looks for lines, just in a lot of detail (so handling up to like x^3 for ~ or something) could work well too. I recomend HoughLinesP() -- returns a list of the lines found. So even U would be like 3 lines prolly

static void linesToAscii() {
	//Okay, so now look into making some ascii art
	//pick the grid for the art... Select the height and then calculate the width
	int ascWidth = (int)(lenWidRatio * (double)(ascHeight) * (((double)src.cols) / ((double)src.rows)));
	// figure out how many pixels to each character -- width and height. This may cause some drift... idk if it will be noticeable
	// add one extra to account for roudning down with integer arithmatic
	int pixHeight = src.rows/(ascHeight)+1; 
	int pixWidth = src.cols/(ascWidth)+1;

	// Now make a grid of that length
	char* ascArt = (char*)malloc(sizeof(char) * ascHeight * ascWidth);
	// now just need to fill it in
	// TODO: the way I am doing it now, there is no reason to store it in an array. Consider buffering for better performance
#ifdef DEBUG_MODE
	printf("    0    5    10   15   20   25   30   35   40   45   50\n");
#endif
	// NOTE: Unlike the line methods, i is Y and j is X... FIXME: swap them, this confusesme repeatedly
	//for(int i = 13 ; i < 14 ; i++){ // to debug one chunk
	for (int i = 0; i < ascHeight; i++) {
#ifdef DEBUG_MODE
		printf("%-3d ", i);
#endif
		//for(int j = 0; j < 10 ; j++){ // to debug one chunk
		for (int j = 0; j < ascWidth; j++) {
			// get the section of the image corrisponding to the character
			// make sure to round on edges
			int yMax = pixHeight * (i + 1) - 1;
			if (yMax >= src.rows) yMax = src.rows-1;
			int xMax = pixWidth * (j + 1) - 1;
			if (xMax >= src.cols) xMax = src.cols-1;
			char toUse = lineChar(pixWidth * j, xMax, pixHeight * i, yMax);
			if (i == 0 || i == ascHeight - 1 || j == 0 || j == ascWidth - 1)
				ascArt[i * ascWidth + j] = (char) toUse;
			else ascArt[i * ascWidth + j] = ' ';
			//printf("%c", ascArt[i * ascWidth + j]);
			printf("%c", toUse);
		}
		printf("\n");
	}

	// TODO: buffer and then do a second pass

	// okay, now I need to delete it
	delete ascArt;
}



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
//				printf("#");
				lit++;
			}
//			else printf(" ");
			all++;
		}
//		printf("\n");
	}
//	printf("----------------------------");
	// print even if just a single line
	if (lit > 0) {
//		printf("lit: %f out of %f --> %d\n", lit, all, (lit / all) >= (xMax - xMin) / all);
	}
	return (lit / all) >= (xMax-xMin)/all;
}

// uses a breadthfirst search to trace consecuative outlines. 
// FIXME: tweak to read off of a double sized pixel grid and return the normal one
static void breadthfirstTrace(char* result, int ascWidth, int ascHeight, int xStart, int pixWidth, int yStart, int pixHeight) {
	std::queue<std::pair<int, int>> pos;

	// make sure the currnet space is not blank
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
			// if you want to get real fancy, could even consider whether there are pixels clustered in the 8 spots and only link with thosewhere that is true (maybe too complex...?)
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


// performs a breadth first search of the image broken up into squares, drawing lines to connect adjacent regions with white pixels
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

	// Now make a grid of that length (final) and double in both dimensions (frst pass)
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


static void CannyThreshold(int, void*)
{
	if (blurThreshold == 0) blurThreshold = 1;
	blur(src_gray, detected_edges, Size(blurThreshold, blurThreshold));
	Canny(detected_edges, detected_edges, lowThreshold, lowThreshold * ratio, kernel_size);
	imshow(window_name, detected_edges);
	outlineToAscii();
}

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