QT += core gui widgets

CONFIG += qt c++11


SOURCES += \
    ../../../src/ColorSpaces.cpp \
    ../../../src/ImageProcessing.cpp \
    ../../../src/main.cpp \
	../../../src/JPEGBitStreamWriter.cpp \
	../../../src/JPEG.cpp \
	../../../src/NxNDCT.cpp
	

HEADERS += \
    ../../../inc/ColorSpaces.h \
    ../../../inc/ImageProcessing.h \
	../../../inc/JPEGBitStreamWriter.h \
	../../../inc/JPEG.h \
	../../../inc/NxNDCT.h

INCLUDEPATH += ../../../lib_inc/ ../../../inc/


unix:!macx|win32: LIBS += -L$$PWD/../../../lib/linux-gcc-x64/ -lImageDSPLib

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/../../../lib/ImageDSPLib.lib
else:unix:!macx|win32-g++: PRE_TARGETDEPS += $$PWD/../../../lib/linux-gcc-x64/libImageDSPLib.a

