#include "JPEG.h"
#include "NxNDCT.h"
#include "math.h"
#include "JPEGBitStreamWriter.h"

#define DEBUG(x) do{ qDebug() << #x << " = " << x;}while(0)

// quantization tables from JPEG Standard, Annex K
const uint8_t QuantLuminance[8*8] =
    { 16, 11, 10, 16, 24, 40, 51, 61,
      12, 12, 14, 19, 26, 58, 60, 55,
      14, 13, 16, 24, 40, 57, 69, 56,
      14, 17, 22, 29, 51, 87, 80, 62,
      18, 22, 37, 56, 68,109,103, 77,
      24, 35, 55, 64, 81,104,113, 92,
      49, 64, 78, 87,103,121,120,101,
      72, 92, 95, 98,112,100,103, 99 };

const uint8_t QuantChrominance[8*8] =
    { 17, 18, 24, 47, 99, 99, 99, 99,
      18, 21, 26, 66, 99, 99, 99, 99,
      24, 26, 56, 99, 99, 99, 99, 99,
      47, 66, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99 };

uint8_t* quant_luminance = new uint8_t[8*8];
uint8_t* quant_chrominance = new uint8_t[8*8];

uint8_t quantQuality(uint8_t quant, uint8_t quality) {
	// Convert to an internal JPEG quality factor, formula taken from libjpeg
	int16_t q = quality < 50 ? 5000 / quality : 200 - quality * 2;
	return clamp((quant * q + 50) / 100, 1, 255);
}

//JPEGBitStreamWriter streamer("example.jpg");
void performJPEGEncoding(uchar Y_buff[], char U_buff[], char V_buff[], int xSize, int ySize, int quality)
{
    //Block size
    int N = 8;

	DEBUG(quality);

	auto s = new JPEGBitStreamWriter("example.jpg");

    //Buffers and pointers

    //Pointers to quantization arrays after the Zig Zag transormation
    uint8_t* zig_zag_luminance;
    uint8_t* zig_zag_chrominance;

    //Buffers for extendBorders function
    int newXSize1;
    int newYSize1;

    int newXSize2;
    int newYSize2;

    int newXSize3;
    int newYSize3;

    char* output_Y;
    char* output_U;
    char* output_V;

    //NxN buffers for one input block
    char* inBlockY1 = new char[N*N];
    char* inBlockY2 = new char[N*N];
    char* inBlockY3 = new char[N*N];
    char* inBlockY4 = new char[N*N];
    char* inBlockU = new char[N*N];
    char* inBlockV = new char[N*N];

    //DCT Kernel buffer
    double* DCTKernel = new double[N*N];

    //Buffers for DCT coeffs
    short* dctCoeffsY1 = new short[N*N];
    short* dctCoeffsY2 = new short[N*N];
    short* dctCoeffsY3 = new short[N*N];
    short* dctCoeffsY4 = new short[N*N];
    short* dctCoeffsU = new short[N*N];
    short* dctCoeffsV = new short[N*N];

    //ZigZag buffers
    short* zig_zag_outputY1;
    short* zig_zag_outputY2;
    short* zig_zag_outputY3;
    short* zig_zag_outputY4;
    short* zig_zag_outputU;
    short* zig_zag_outputV;

    //1. Zig Zag transformations for quantization matrixes

    for (int j = 0; j < N; j++)
    {
        for (int i = 0; i < N; i++)
        {
            quant_luminance[j*N+i] = quantQuality(QuantLuminance[j*N+i], quality);
            quant_chrominance[j*N+i] = quantQuality(QuantChrominance[j*N+i], quality);
        }
    }

    doZigZag2(quant_luminance, N, &zig_zag_luminance);
    doZigZag2(quant_chrominance, N, &zig_zag_chrominance);

    //2. Change Y_buff values(not the type) from uchar to char
    char* newYbuff = new char[xSize * ySize];

    for (int j = 0; j < ySize; j++)
    {
        for (int i = 0; i < xSize; i++){
            newYbuff[j * xSize + i] = Y_buff[j * xSize + i] - 128.0;
        }
    }

    //3. Extend borders - Does not have to be done because only 1 image has the invalid dimensions
    //                      and after this function there is some noise on the bottom of new image


    //Mozda prvo prosiri pa onda prebaci u char xe xe
    if(xSize*ySize % 16 != 0){
        extendBorders(newYbuff, xSize, ySize, 8, &output_Y, &newXSize1, &newYSize1);
        extendBorders(U_buff, xSize/2, ySize/2, 8, &output_U, &newXSize2, &newYSize2);
        extendBorders(V_buff, xSize/2, ySize/2, 8, &output_V, &newXSize3, &newYSize3);
    }else{
        output_Y = newYbuff;
        newXSize1 = xSize;
        newYSize1 = ySize;

        output_U = U_buff;
        newXSize2 = xSize/2;
        newYSize2 = ySize/2;

        output_V = V_buff;
        newXSize3 = xSize/2;
        newYSize3 = ySize/2;}

    //4. Write the basic header info to JPEG file
    s->writeHeader();
    s->writeQuantizationTables((uint8_t*)zig_zag_luminance, (uint8_t*)zig_zag_chrominance);
    s->writeImageInfo(newXSize1, newYSize1);
    s->writeHuffmanTables();

    //5. DCT kernel assignment
    GenerateDCTmatrix(DCTKernel, N);

    //The loop
    for (int y = 0, y_uv = 0; y < newYSize1/N; y+=2, y_uv++)
    {
        for (int x = 0, x_uv = 0; x < newXSize1/N; x+=2, x_uv++)
        {
            //Iterating through 4 Y blocks and 1 U and V block

            //6. InBlock filling
            for (int j = 0; j < N; j++)
            {
                for (int i = 0; i < N; i++)
                {
                    //Y1 - upper-left
                    //Y2 - upper-right
                    //Y3 - lower-left
                    //Y4 - lower-right
                    inBlockY1[j*N+i] = output_Y[(y*N + j)*newXSize1 + x*N + i];
                    inBlockY2[j*N+i] = output_Y[(y*N + j)*newXSize1 + (x+1)*N + i];
                    inBlockY3[j*N+i] = output_Y[((y+1)*N + j)*newXSize1 + x*N + i];
                    inBlockY4[j*N+i] = output_Y[((y+1)*N + j)*newXSize1 + (x+1)*N + i];
                    inBlockU[j*N+i] = output_U[(y_uv*N + j)*newXSize2 + x_uv*N + i];
                    inBlockV[j*N+i] = output_V[(y_uv*N + j)*newXSize3 + x_uv*N + i];
                }
            }

            //7. DCT
            DCT(inBlockY1, dctCoeffsY1, N, DCTKernel);
            DCT(inBlockY2, dctCoeffsY2, N, DCTKernel);
            DCT(inBlockY3, dctCoeffsY3, N, DCTKernel);
            DCT(inBlockY4, dctCoeffsY4, N, DCTKernel);
            DCT(inBlockU, dctCoeffsU, N, DCTKernel);
            DCT(inBlockV, dctCoeffsV, N, DCTKernel);

            //8. Quantization
            for (int j = 0; j < N; j++)
            {
                for (int i = 0; i < N; i++)
                {
                    dctCoeffsY1[j*N+i] = round(dctCoeffsY1[j*N+i] / quant_luminance[j*N+i]);
                    dctCoeffsY2[j*N+i] = round(dctCoeffsY2[j*N+i] / quant_luminance[j*N+i]);
                    dctCoeffsY3[j*N+i] = round(dctCoeffsY3[j*N+i] / quant_luminance[j*N+i]);
                    dctCoeffsY4[j*N+i] = round(dctCoeffsY4[j*N+i] / quant_luminance[j*N+i]);
                    dctCoeffsU[j*N+i]  = round(dctCoeffsU[j*N+i] / quant_chrominance[j*N+i]);
                    dctCoeffsV[j*N+i]  = round(dctCoeffsV[j*N+i] / quant_chrominance[j*N+i]);
                }
            }

            //9. ZigZag
            doZigZag1(dctCoeffsY1, N, &zig_zag_outputY1);
            doZigZag1(dctCoeffsY2, N, &zig_zag_outputY2);
            doZigZag1(dctCoeffsY3, N, &zig_zag_outputY3);
            doZigZag1(dctCoeffsY4, N, &zig_zag_outputY4);
            doZigZag1(dctCoeffsU, N, &zig_zag_outputU);
            doZigZag1(dctCoeffsV, N, &zig_zag_outputV);

            //10. Write blocks
            s->writeBlockY(zig_zag_outputY1);
            s->writeBlockY(zig_zag_outputY2);
            s->writeBlockY(zig_zag_outputY3);
            s->writeBlockY(zig_zag_outputY4);
            s->writeBlockU(zig_zag_outputU);
            s->writeBlockV(zig_zag_outputV);
        }
    }

    //11. Finishing JPEG file write
    s->finishStream();

    //Delete used memory buffers coefficients
    delete[] inBlockY1;
    delete[] inBlockY2;
    delete[] inBlockY3;
    delete[] inBlockY4;
    delete[] inBlockU;
    delete[] inBlockV;
    delete[] DCTKernel;
    delete[] dctCoeffsY1;
    delete[] dctCoeffsY2;
    delete[] dctCoeffsY3;
    delete[] dctCoeffsY4;
    delete[] dctCoeffsU;
    delete[] dctCoeffsV;

	delete s;
}

void doZigZag1(short block[], int N, short** p_output){
    int currDiagonalWidth = 1;
    int row, col;
    int brojac = 0;

    short* output = new short[N*N];

    while (currDiagonalWidth<N)
    {
        for (int i = 0; i<currDiagonalWidth; i++)
        {
            if (currDiagonalWidth % 2 == 1)
            {
                row = currDiagonalWidth - i - 1;
                col = i;
            }
            else
            {
                row = i;
                col = currDiagonalWidth - i - 1;
            }

            output[brojac] = block[row * N + col];
            brojac++;
        }

        currDiagonalWidth++;
    }

    while (currDiagonalWidth> 0)
    {
        for (int i = currDiagonalWidth; i> 0; i--)
        {

            if (currDiagonalWidth % 2 == 1)
            {
                row = N - currDiagonalWidth + i - 1;
                col = N - i;
            }
            else
            {
                row = N - i;
                col = N - currDiagonalWidth + i - 1;
            }

            output[brojac] = block[row * N + col];
            brojac++;
        }
        currDiagonalWidth--;
    }

    *p_output = output;
}

void doZigZag2(uint8_t block[], int N, uint8_t** p_output){
    int currDiagonalWidth = 1;
    int row, col;
    int brojac = 0;

    uint8_t* output = new uint8_t[N*N];

    while (currDiagonalWidth<N)
    {
        for (int i = 0; i<currDiagonalWidth; i++)
        {
            if (currDiagonalWidth % 2 == 1)
            {
                row = currDiagonalWidth - i - 1;
                col = i;
            }
            else
            {
                row = i;
                col = currDiagonalWidth - i - 1;
            }

            output[brojac] = block[row * N + col];
            brojac++;
        }

        currDiagonalWidth++;
    }

    while (currDiagonalWidth> 0)
    {
        for (int i = currDiagonalWidth; i> 0; i--)
        {

            if (currDiagonalWidth % 2 == 1)
            {
                row = N - currDiagonalWidth + i - 1;
                col = N - i;
            }
            else
            {
                row = N - i;
                col = N - currDiagonalWidth + i - 1;
            }

            output[brojac] = block[row * N + col];
            brojac++;
        }
        currDiagonalWidth--;
    }

    *p_output = output;
}
