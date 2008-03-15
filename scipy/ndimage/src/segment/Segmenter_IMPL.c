#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ndImage_Segmenter_structs.h"

// these are for this standalone and come out with the full build
//
#define MAX(a, b) ((a) > (b) ? (a) : (b)) 
#define FALSE 0
#define TRUE  1


int NI_EdgePreFilter(int num, int rows, int cols, int lowThreshold, int highThreshold,
                     int aperature, int HalfFilterTaps, unsigned short *sImage, double *dImage, double *kernel){

	int i, j, k, n, num1;
    	int offset;
	double sum, value;
	double *buffer;
	int max_buffer = MAX(rows, cols);
	int status;

	buffer = calloc(max_buffer+aperature+16, sizeof(double));

	num1 = HalfFilterTaps;
	offset = 0;
	for(i = 0; i < rows; ++i){
	    /* copy image row to local buffer  */
	    for(j = 0; j < cols; ++j){
		buffer[num1+j] = sImage[offset+j];
	    }
	    /* constant pad the ends of the buffer */
	    for(j = 0; j < num1; ++j){
		buffer[j] = buffer[num1];
	    }
	    for(j = cols+num1; j < cols+2*num1; ++j){
		buffer[j] = buffer[cols-1+num1];
	    }

	    /* Perform Symmetric Convolution in the X dimension. */
	    for(n = 0, j = num1; j < (cols+num1); ++j, ++n){
	        sum = buffer[j] * kernel[num1];
	        for(k = 1; k < num1; ++k){
	            sum += kernel[num1-k] * (buffer[j+k] + buffer[j-k]);
	        }
	        dImage[offset+n] = sum;
	    }
	    offset += cols;
	}

	offset = 0;
	for(i = 0; i < cols; ++i){
	    /* copy image column to local buffer */
	    offset = 0;
	    for(j = 0; j < rows; ++j){
            buffer[num1+j] = dImage[offset+i];
	        offset += cols;
	    }
	    /* constant pad the ends of the buffer */
	    for(j = 0; j < num1; ++j){
		buffer[j] = buffer[num1];
	    }
	    for(j = rows+num1; j < rows+2*num1; ++j){
	        buffer[j] = buffer[rows-1+num1];
	    }

	    /* Perform Symmetric Convolution in the Y dimension. */
	    offset = 0;
	    for(j = num1; j < (rows+num1); ++j){
	        sum = buffer[j] * kernel[num1];
	        for(k = 1; k < num1; ++k){
	            sum += kernel[num1-k] * (buffer[j+k] + buffer[j-k]);
	        }
	        dImage[offset+i] = sum;
	        offset += cols;
	    }
	}

	/* threshold the image */
	offset = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		value = dImage[offset+j];
		if(value < (float)lowThreshold)  value = (float)0.0;
		if(value > (float)highThreshold) value = (float)0.0;
		dImage[offset+j] = value;
	    }
	    offset += cols;
	}

	free(buffer);

	status = 1;

	return(status);

}

int NI_SobelImage(int samples, int rows, int cols, double *rawImage, double *edgeImage, double *pAve,
	          int *minValue, int *maxValue){
             
	int i, j;
	int p, m, n;
	int offset;
	int offsetM1;
	int offsetP1;
	int status;
	int count = 0;

	/*
	// Sobel
	*/
	offset = cols;
	*pAve = 0.0;
	*minValue = 10000;
	*maxValue = -10000;
	for(i = 1; i < rows-1; ++i){
	    offsetM1 = offset - cols;
	    offsetP1 = offset + cols;
	    for(j = 1; j < cols-1; ++j){
	        n = 2*rawImage[offsetM1+j] + rawImage[offsetM1+j-1] + rawImage[offsetM1+j+1] -
	            2*rawImage[offsetP1+j] - rawImage[offsetP1+j-1] - rawImage[offsetP1+j+1];
	        m = 2*rawImage[offset+j-1] + rawImage[offsetM1+j-1] + rawImage[offsetP1+j-1] -
	            2*rawImage[offset+j+1] - rawImage[offsetM1+j+1] - rawImage[offsetP1+j+1];
	        p = (int)sqrt((float)(m*m) + (float)(n*n));
		if(p > 0){
		    *pAve += p;
		    if(p > *maxValue) *maxValue = p;
		    if(p < *minValue) *minValue = p;
		    ++count;
		}
	        edgeImage[offset+j] = p;
	    }
	    offset += cols;
	}
	/* threshold based on ave */
	*pAve /= count;

	status = 1;

	return(status);

}


int NI_SobelEdge(int samples, int rows, int cols, double *edgeImage, unsigned short *edges, 
	          int mode, double pAve, int minValue, int maxValue, double sobelLow){

	int i, j;
	int offset;
	int value;
	int maxIndex;
	int status;
	int histogram[256];
	float pThreshold;
	double scale;
	double step;

	scale = 1.0 / maxValue;

	step = 255.0/(maxValue-minValue);
	for(i = 0; i < 256; ++i){
	    histogram[i] = 0;
	}
	offset = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		value = (int)(step*(edgeImage[offset+j]-minValue));
	        ++histogram[value];
	    }
	    offset += cols;
	}

	if(mode == 1){
	    /* based on the mean value of edge energy */
	    pThreshold = (int)(sobelLow * (float)pAve);
	}
	else{
	    /* based on the mode value of edge energy */
	    pThreshold = (sobelLow * (minValue + ((float)maxIndex/step)));
	}

	offset = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		if(edgeImage[offset+j] > pThreshold){
		    edges[offset+j] = 1;
		}
		else{
		    edges[offset+j] = 0;
		}
	    }
	    offset += cols;
	}

	status = 1;
	return(status);

}

int NI_GetBlobs(int samples, int rows, int cols, unsigned short *edges, unsigned short *connectedEdges, int *groups){ 

	int            i, j, k, l, m;
	int            offset;
	int            Label;
	int            status;
	int            Classes[4096];
	bool           NewLabel;
	bool           Change;
	unsigned short T[12];

	/*
	// connected components labeling. pixels touch within 3x3 mask for edge connectedness. 
	*/
	Label  = 1;
	offset = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		connectedEdges[offset+j] = 0; 
		if(edges[offset+j] == 1){
		    connectedEdges[offset+j] = Label++; 
		}
	    }
	    offset += cols;
	}

	while(1){
	    Change = FALSE;
	    /*
	    // TOP-DOWN Pass for labeling
	    */
	    offset = cols;
	    for(i = 1; i < rows-1; ++i){
		for(j = 1; j < cols-1; ++j){
		    if(connectedEdges[offset+j] != 0){
			T[0] = connectedEdges[offset+j];
			T[1] = connectedEdges[offset+j+1];
			T[2] = connectedEdges[offset-cols+j+1];
			T[3] = connectedEdges[offset-cols+j];
			T[4] = connectedEdges[offset-cols+j-1];
			T[5] = connectedEdges[offset+j-1];
			T[6] = connectedEdges[offset+cols+j-1];
			T[7] = connectedEdges[offset+cols+j];
			T[8] = connectedEdges[offset+cols+j+1];
			m = T[0];
			for(l = 1; l < 9; ++l){
			    if(T[l] != 0){
				if(T[l] < m) m = T[l];
			    }
			}
			if(m != connectedEdges[offset+j]){
			    Change = TRUE;
			    connectedEdges[offset+j] = m;
			}
		    }
		}
		offset += cols;
	    }
	    /*
	    // BOTTOM-UP Pass for labeling
	    */
	    offset = (rows-1)*cols;
	    for(i = (rows-1); i > 1; --i){
		for(j = (cols-1); j > 1; --j){
		    if(connectedEdges[offset+j] != 0){
			T[0] = connectedEdges[offset+j];
			T[1] = connectedEdges[offset+j+1];
			T[2] = connectedEdges[offset-cols+j+1];
			T[3] = connectedEdges[offset-cols+j];
			T[4] = connectedEdges[offset-cols+j-1];
			T[5] = connectedEdges[offset+j-1];
			T[6] = connectedEdges[offset+cols+j-1];
			T[7] = connectedEdges[offset+cols+j];
			T[8] = connectedEdges[offset+cols+j+1];
			m = T[0];
			for(l = 1; l < 9; ++l){
			    if(T[l] != 0){
				if(T[l] < m) m = T[l];
			    }
			}
			if(m != connectedEdges[offset+j]){
			    Change = TRUE;
			    connectedEdges[offset+j] = m;
			}
		    }
		}
		offset -= cols;
	    }
	    if(!Change) break;
	}   /* end while loop */

	Classes[0] = 0;
	Label      = 1;
	offset     = cols;
	for(i = 1; i < (rows-1); ++i){
	    for(j = 1; j < (cols-1); ++j){
		m = connectedEdges[offset+j];
		if(m > 0){
		    NewLabel = TRUE;
		    for(k = 1; k < Label; ++k){
			if(Classes[k] == m) NewLabel = FALSE;
		    }
		    if(NewLabel){
			Classes[Label++] = m;
			if(Label > 4000){
			    return 0; /* too many labeled regions. this is a pathology */
			}
		    }
		}
	    }
	    offset += cols;
	}

	/*
	// re-label the connected blobs in continuous label order
	*/
	offset = cols;
	for(i = 1; i < (rows-1); ++i){
	    for(j = 1; j < (cols-1); ++j){
		m = connectedEdges[offset+j];
		if(m > 0){
		    for(k = 1; k < Label; ++k){
			if(Classes[k] == m){
			    connectedEdges[offset+j] = (unsigned short)k;
			    break;
			}
		    }
		}
	    }
	    offset += cols;
	}

	*groups = Label;

	/*
	// prune the isolated pixels
	*/
	offset  = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		if(connectedEdges[offset+j] > (*groups)){
		    connectedEdges[offset+j] = 0;
		}	
	    }
	    offset  += cols;
	}

	status = 1;
	return(status);

}

int NI_GetBlobRegions(int rows, int cols, int numberObjects, unsigned short *labeledEdges,
                      objStruct objectMetrics[]){

	int i, j, k, m;
	int offset;
	int count;
	int LowX;
	int LowY;
	int HighX;
	int HighY;
	int status;
	float centerX;
	float centerY;

	for(k = 1; k < numberObjects; ++k){
	    offset     = cols;
	    LowX       = 32767;
	    LowY       = 32767;
	    HighX      = 0;
	    HighY      = 0;
	    count      = 0;
	    centerX    = (float)0.0;
	    centerY    = (float)0.0;
	    for(i = 1; i < (rows-1); ++i){
		for(j = 1; j < (cols-1); ++j){
		    m = labeledEdges[offset+j];
		    if(k == m){
			if(i < LowY)   LowY = i;
			if(j < LowX)   LowX = j;
			if(i > HighY) HighY = i;
			if(j > HighX) HighX = j;
	    		centerX += (float)j;
	    		centerY += (float)i;
	    		++count;
		    }
		}
		offset += cols;
	    }
	    /* the bounding box for the 2D blob */
	    objectMetrics[k-1].L     = LowX;
	    objectMetrics[k-1].R     = HighX;
	    objectMetrics[k-1].B     = LowY;
	    objectMetrics[k-1].T     = HighY;
	    objectMetrics[k-1].Area  = count;
	    objectMetrics[k-1].cX    = centerX/(float)count;
	    objectMetrics[k-1].cY    = centerY/(float)count;
	    objectMetrics[k-1].Label = k;
	}

	status = numberObjects;
	return status;

}


void NI_ThinMorphoFilter(int regRows, int regColumns, int spadSize, int masks, unsigned short *J_mask, 
	                 unsigned short *K_mask, unsigned char *Input, unsigned char *CInput, 
	                 unsigned char *ErosionStage, unsigned char *DialationStage, 
		         unsigned char *HMT, unsigned char *Copy){

	int i, j, k, l, m, n, overlap, hit;
	int LowValue1, HighValue1;   
	int LowValue2, HighValue2;   
	int Column, T, nloop;
	int Offset;
	int N, M;
	int maskCols = 3;
	int j_mask[3][3];
	int k_mask[3][3];
	int status;

	N = regRows;
	M = regColumns;

	LowValue1  = 1;   
	HighValue1 = 0;   

	LowValue2  = 0;   
	HighValue2 = 1;   

	Offset = 0;
	for(i = 0; i < N; ++i){
	    for(j = 0; j < M; ++j){
		Copy[Offset+j] = Input[Offset+j];
	    }
	    Offset += spadSize;
	}

	nloop = 0;
	while(1){
	    /* erode */
	    Column = 0;
	    for(n = 0; n < masks; ++n){
		for(i = 0; i < 3; ++i){
		    for(j = 0; j < 3; ++j){
			j_mask[i][j] = J_mask[i+maskCols*(Column+j)];
		    }
		}
		for(i = 0; i < 3; ++i){
		    for(j = 0; j < 3; ++j){
			k_mask[i][j] = K_mask[i+maskCols*(Column+j)];
		    }
		}
		Column += 3;

		Offset = spadSize;
		for(i = 1; i < N-1; ++i){
		    for(j = 1; j < M-1; ++j){
			hit = LowValue1; 
			for(k = -1; k < 2; ++k){
			    for(l = -1; l < 2; ++l){
				T = j_mask[k+1][l+1];
				if(T == 1){
				    overlap = T*Input[Offset+(k*spadSize)+j+l];
				    if(overlap == HighValue1) hit = HighValue1;
				}
			    }
			}
			ErosionStage[Offset+j] = hit;
		    }
		    Offset += spadSize;
		}

		/* dialate */
		Offset = 0;
		for(i = 0; i < N; ++i){
		    for(j = 0; j < M; ++j){
			CInput[Offset+j] = (~Input[Offset+j]) & 0x1; 
		    }
		    Offset += spadSize;
		}

		Offset = spadSize;
		for(i = 1; i < N-1; ++i){
		    for(j = 1; j < M-1; ++j){
			hit = LowValue1; 
			for(k = -1; k < 2; ++k){
			    for(l = -1; l < 2; ++l){
				T = k_mask[k+1][l+1];
				if(T == 1){
				    overlap = T*CInput[Offset+(k*spadSize)+j+l];
				    if(overlap == HighValue1) hit = HighValue1;
			        }
			    }
			}
			DialationStage[Offset+j] = hit;
		    }
		    Offset += spadSize;
		}

		/* form the HMT */
		Offset = 0;
		for(i = 0; i < N; ++i){
		    for(j = 0; j < M; ++j){
			m = (ErosionStage[Offset+j]*DialationStage[Offset+j]);
			HMT[Offset+j] = m;
		    }
		    Offset += spadSize;
		}

		/* Thin for stage n */

		Offset = 0;
		for(i = 0; i < N; ++i){
		    for(j = 0; j < M; ++j){
			HMT[Offset+j] = (~HMT[Offset+j]) & 0x1; 
		    }
		    Offset += spadSize;
		}

		Offset = 0;
		for (i = 0; i < N; ++i){
		    for (j = 0; j < M; ++j){
			m = (Input[Offset+j]*HMT[Offset+j]);
			Input[Offset+j] = m;
		    }
		    Offset += spadSize;
		}
	    }

	    /* check for no change */
	    hit = 0;
	    Offset = 0;
	    for(i = 0; i < N; ++i){
		for(j = 0; j < M; ++j){
		    hit += abs(Copy[Offset+j]-Input[Offset+j]);
		}
		Offset += spadSize;
	    }
	    if(!hit) break;

	    hit = 0;
	    Offset = 0;
	    for(i = 0; i < N; ++i){
		for(j = 0; j < M; ++j){
		    Copy[Offset+j] = Input[Offset+j];
		    if(Input[Offset+j]) ++hit;
		}
		Offset += spadSize;
	    }
	    /* nloop is data dependent. */
	    ++nloop;
	}


	status = 1;
	return status;

}


int NI_CannyFilter(int samples, int rows, int cols, double *rawImage,
		   double *hDGImage, double *vDGImage, double *dgKernel, 
                   int gWidth, float *aveXValue, float *aveYValue){
               

	/*
	// implements the derivative of Gaussian filter. kernel set by CannyEdges
	*/
	int i, j, k;
	int ptr;
	int mLength;
	int count;
	int status;
	float *tBuffer = NULL;
	double sum;

	*aveXValue = (float)0.0;
	*aveYValue = (float)0.0;	

	mLength = MAX(rows, cols) + 64;
	tBuffer = calloc(mLength, sizeof(float));

	/*
	// filter X 
	*/
	count = 0;
	for(i = 0; i < rows; ++i){
	    ptr = i * cols;
	    for(j = gWidth; j < cols-gWidth; ++j){
		sum = dgKernel[0] * rawImage[ptr+j];
		for(k = 1; k < gWidth; ++k){
		    sum += dgKernel[k] * (-rawImage[ptr+j+k] + rawImage[ptr+j-k]);
		}
		hDGImage[ptr+j] = (float)sum;
		if(sum != (float)0.0){
		    ++count;
		    *aveXValue += (float)fabs(sum);
		}
	    }
	}
	if(count){
	    *aveXValue /= (float)count;
	}
	/*
	// filter Y 
	*/
	count = 0;
	for(i = 0; i < cols; ++i){
	    for(j = 0; j < rows; ++j){
		ptr = j * cols;
		tBuffer[j] = rawImage[ptr+i];
	    }
	    for(j = gWidth; j < rows-gWidth; ++j){
		ptr = j * cols;
		sum = dgKernel[0] * tBuffer[j];
		for(k = 1; k < gWidth; ++k){
		    sum += dgKernel[k] * (-tBuffer[j+k] + tBuffer[j-k]);
		}
		vDGImage[ptr+i] = sum;
		if(sum != (float)0.0){
		    ++count;
		    *aveYValue += (float)fabs(sum);
		}
	    }
	}
	if(count){
	    *aveYValue /= (float)count;
	}

	free(tBuffer);

	status = 1;

	return status;

}

double tmagnitude(double X, double Y){
	return sqrt(X*X + Y*Y);
}

int NI_CannyNonMaxSupress(int num, int rows, int cols, double *magImage, double *hDGImage,
	                  double *vDGImage, int mode, double aveXValue, double aveYValue,
			  double *tAve, double *cannyLow, double *cannyHigh, 
			  double cannyL, double cannyH){
                   
	int i, j;
	int ptr, ptr_m1, ptr_p1;
	float xSlope, ySlope, G1, G2, G3, G4, G, xC, yC;
	float scale;
	float maxValue = (float)0.0;
	float minValue = (float)0.0;
	int value;
	int mValue;
	int mIndex;
	int count;
	int status;
	int histogram[256];
	double step;

	for(i = 1; i < rows-1; ++i){
	    ptr = i * cols;
	    ptr_m1 = ptr - cols;
	    ptr_p1 = ptr + cols;
	    for(j = 1; j < cols; ++j){
		magImage[ptr+j] = (float)0.0;
		xC = hDGImage[ptr+j];
		yC = vDGImage[ptr+j];
		if(!((fabs(xC) < aveXValue) && (fabs(yC) < aveYValue))){
		    G = tmagnitude(xC, yC);
		    if(fabs(yC) > fabs(xC)){
		        /* vertical gradient */
		        xSlope = (float)(fabs(xC) / fabs(yC));
		        ySlope = (float)1.0;
		        G2 = tmagnitude(hDGImage[ptr_m1+j], vDGImage[ptr_m1+j]);
		        G4 = tmagnitude(hDGImage[ptr_p1+j], vDGImage[ptr_p1+j]);	
		        if((xC*yC) > (float)0.0){
			    G1 = tmagnitude(hDGImage[ptr_m1+j-1], vDGImage[ptr_m1+j-1]);
			    G3 = tmagnitude(hDGImage[ptr_p1+j+1], vDGImage[ptr_p1+j+1]);
		        }
		        else{
			    G1 = tmagnitude(hDGImage[ptr_m1+j+1], vDGImage[ptr_m1+j+1]);
			    G3 = tmagnitude(hDGImage[ptr_p1+j-1], vDGImage[ptr_p1+j-1]);
		        }
		    }
		    else{
		        /* horizontal gradient */
		        xSlope = (float)(fabs(yC) / fabs(xC));
		        ySlope = (float)1.0;
		        G2 = tmagnitude(hDGImage[ptr+j+1], vDGImage[ptr+j+1]);
		        G4 = tmagnitude(hDGImage[ptr+j-1], vDGImage[ptr+j-1]);	
		        if((xC*yC) > (float)0.0){
			    G1 = tmagnitude(hDGImage[ptr_p1+j+1], vDGImage[ptr_p1+j+1]);
			    G3 = tmagnitude(hDGImage[ptr_m1+j-1], vDGImage[ptr_m1+j-1]);
		        }
		        else{
			    G1 = tmagnitude(hDGImage[ptr_m1+j+1], vDGImage[ptr_m1+j+1]);
			    G3 = tmagnitude(hDGImage[ptr_p1+j-1], vDGImage[ptr_p1+j-1]);
		        }
		    }
		    if((G > (xSlope*G1+(ySlope-xSlope)*G2))&&(G > (xSlope*G3+(ySlope-xSlope)*G4))){
		        magImage[ptr+j] = G;	
		    }
		    if(magImage[ptr+j] > maxValue) maxValue = magImage[ptr+j];
		    if(magImage[ptr+j] < minValue) minValue = magImage[ptr+j];
		}
	    }
	}

	scale = (float)1.0 / (maxValue-minValue);
	ptr   = 0;
	count = 0;
	*tAve  = 0.0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		magImage[ptr] = scale * (magImage[ptr]-minValue);
		if(magImage[ptr] > 0.0){
		    *tAve += magImage[ptr];
		    ++count;
		}
		++ptr;
	    }
	}
	*tAve /= (float)count;

	step = 255.0;
	for(i = 0; i < 256; ++i){
	    histogram[i] = 0;
	}
	ptr = 0;
	for(i = 0; i < rows; ++i){
	    for(j = 0; j < cols; ++j){
		value = (int)(step*(magImage[ptr]));
	        ++histogram[value];
		++ptr;
	    }
	}
	/*
	// now get the max after skipping the low values
	*/
	mValue = -1;
	mIndex = 0;
	for(i = 10; i < 256; ++i){
	    if(histogram[i] > mValue){
		mValue = histogram[i];
		mIndex = i;
	    }
	}

	if(mode == 1){
	    /* based on the mean value of edge energy */
	    *cannyLow  = ((cannyL)  * *tAve);
	    *cannyHigh = ((cannyH) * *tAve);
	}
	else{
	    /* based on the mode value of edge energy */
	    *cannyLow  = ((cannyL)  * ((float)mIndex/step));
	    *cannyHigh = ((cannyH) * ((float)mIndex/step));
	}
	status = 1;

	return status;

}

int trace_Edge(int i, int j, int rows, int cols, double cannyLow, double *magImage,
               unsigned short *hys_image){

	int n, m;
	int ptr;
	int flag;

	ptr = i * cols;
	if(hys_image[ptr+j] == 0){
	    /*
	    // this point is above high threshold
	    */
	    hys_image[ptr+j] = 1;
	    flag = 0;
	    for(n = -1; n <= 1; ++n){
		for(m = -1; m <= 1; ++m){
		    if(n == 0 && m == 0) continue;
		    if(((i+n) > 0) && ((j+m) > 0) && ((i+n) < rows) && ((j+m) < cols)){
			ptr = (i+n) * cols;
			if(magImage[ptr+j+m] > cannyLow){
	    		    /*
	    		    // this point is above low threshold
	    		    */
			    if(trace_Edge(i+n, j+m, rows, cols, cannyLow, magImage, hys_image)){
				flag = 1;
				break;
			    }
			}
		    }
		}
		if(flag) break;
	    }
	    return(1);
	}

	return(0);

}
int NI_CannyHysteresis(int num, int rows, int cols, double *magImage, unsigned short *hys_image,
		       double cannyLow, double cannyHigh){ 


	int status;
	int i, j;
	int ptr;

	for(i = 0; i < rows; ++i){
	    ptr = i * cols;
	    for(j = 0; j < cols; ++j){
		if(magImage[ptr+j] > cannyHigh){
		    trace_Edge(i, j, rows, cols, cannyLow, magImage, hys_image);
		}
	    }
	}


	status = 1;

	return status;

}



