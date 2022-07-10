
#include "ImageProcessing.h"
#include "ColorSpaces.h"
#include "JPEG.h"

#include <cmath>

#include <QDebug>
#include <QString>
#include <QImage>

#define COMPRESS_420 1

void imageProcessingFun(const QString& progName, QImage& outImgs, const QImage& inImgs, const QVector<double>& params)
{
	/* Create buffers for YUV image */
	uchar* Y_buff = new uchar[inImgs.width()*inImgs.height()];
#if COMPRESS_420
    char* U_buff = new char[inImgs.width()*inImgs.height()/4];
    char* V_buff = new char[inImgs.width()*inImgs.height()/4];
#else
    char* U_buff = new char[inImgs.width()*inImgs.height()];
    char* V_buff = new char[inImgs.width()*inImgs.height()];
#endif

	int X_SIZE = inImgs.width();
	int Y_SIZE = inImgs.height();

	/* Create empty output image */
	outImgs = QImage(inImgs.width(), inImgs.height(), inImgs.format());

	/* Convert input image to YUV420 image */
#if COMPRESS_420
    RGBtoYUV420(inImgs.bits(), X_SIZE, Y_SIZE, Y_buff, U_buff, V_buff);
#else
    RGBtoYUV444(inImgs.bits(), X_SIZE, Y_SIZE, Y_buff, U_buff, V_buff);
#endif

    if(progName == QString("JPEG Encoder"))
	{	
		/* Perform NxN DCT with block size 8 */
        performJPEGEncoding(Y_buff, U_buff, V_buff, X_SIZE, Y_SIZE, params[0]);
	}

#if COMPRESS_420
    /* Zero out U and V component. */
    //procesing_YUV420(Y_buff, U_buff, V_buff, inImgs.width(), inImgs.height(), 1, 0, 0);

    /* Convert YUV image back to RGB */
    YUV420toRGB(Y_buff, U_buff, V_buff, inImgs.width(), inImgs.height(), outImgs.bits());
#else
    YUV444toRGB(Y_buff, U_buff, V_buff, inImgs.width(), inImgs.height(), outImgs.bits());
#endif

    outImgs = QImage("example.jpg");

	/* Delete used memory buffers */
	delete[] Y_buff;
	delete[] U_buff;
	delete[] V_buff;

}

