#ifndef JPEG_H_
#define JPEG_H_

#include <QDebug>


void performJPEGEncoding(uchar Y_buff[], char U_buff[], char V_buff[], int xSize, int ySize, int compressionRate);
void doZigZag1(short block[], int N, short** p_output);
void doZigZag2(uint8_t block[], int N, uint8_t** p_output);

#endif // JPEG_H_

