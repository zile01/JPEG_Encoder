

#include "JPEGBitStreamWriter.h"


// Huffman definitions for first DC/AC tables (luminance / Y channel)
const uint8_t DcLuminanceCodesPerBitsize[16]   = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };   // sum = 12
const uint8_t DcLuminanceValues         [12]   = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes
const uint8_t AcLuminanceCodesPerBitsize[16]   = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 }; // sum = 162
const uint8_t AcLuminanceValues        [162]   =                                        // => 162 codes
    { 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08, // 16*10+2 symbols because
      0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28, // upper 4 bits can be 0..F
      0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59, // while lower 4 bits can be 1..A
      0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89, // plus two special codes 0x00 and 0xF0
      0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6, // order of these symbols was determined empirically by JPEG committee
      0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
      0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };
// Huffman definitions for second DC/AC tables (chrominance / Cb and Cr channels)
const uint8_t DcChrominanceCodesPerBitsize[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };   // sum = 12
const uint8_t DcChrominanceValues         [12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes (identical to DcLuminanceValues)
const uint8_t AcChrominanceCodesPerBitsize[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 }; // sum = 162
const uint8_t AcChrominanceValues        [162] =                                        // => 162 codes
    { 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91, // same number of symbol, just different order
      0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26, // (which is more efficient for AC coding)
      0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,
      0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
      0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,
      0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
      0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };



void JPEGBitStreamWriter::writeHeader() {
	  // ////////////////////////////////////////
	  // JFIF headers
	  const uint8_t HeaderJfif[2+2+16] =
		  { 0xFF,0xD8,         // SOI marker (start of image)
			0xFF,0xE0,         // JFIF APP0 tag
			0,16,              // length: 16 bytes (14 bytes payload + 2 bytes for this length field)
			'J','F','I','F',0, // JFIF identifier, zero-terminated
			1,1,               // JFIF version 1.1
			0,                 // no density units specified
			0,1,0,1,           // density: 1 pixel "per pixel" horizontally and vertically
			0,0 };             // no thumbnail (size 0 x 0)
	  bitWriter << HeaderJfif;
}

void JPEGBitStreamWriter::writeQuantizationTables(
	uint8_t LuminanceQuantTable[64],
	uint8_t ChrominanceQuantTable[64]
) {
	bitWriter.addMarker(0xDB, 2 + (2) * (1 + 8*8));
	// length: 65 bytes per table + 2 bytes for this length field
	// each table has 64 entries and is preceded by an ID byte

	bitWriter << 0x00;
	for(auto i = 0; i < 64; i++){
		bitWriter << LuminanceQuantTable[i];
	}
	bitWriter << 0x01;
	for(auto i = 0; i < 64; i++){
		bitWriter << ChrominanceQuantTable[i];
	}
}

void JPEGBitStreamWriter::writeImageInfo(int width, int height) {
    //TODO vrati na 1 posle
    auto numComponents = 3;
  // ////////////////////////////////////////
  // write image infos (SOF0 - start of frame)
  bitWriter.addMarker(0xC0, 2+6+3*numComponents); // length: 6 bytes general info + 3 per channel + 2 bytes for this length field

  // 8 bits per channel
  bitWriter << 0x08
  // image dimensions (big-endian)
			<< (height >> 8) << (height & 0xFF)
			<< (width  >> 8) << (width  & 0xFF);

  // sampling and quantization tables for each component
  bitWriter << numComponents;       // 1 component (grayscale, Y only) or 3 components (Y,Cb,Cr)
  for (auto id = 1; id <= numComponents; id++)
	bitWriter <<  id                // component ID (Y=1, Cb=2, Cr=3)
	// bitmasks for sampling: highest 4 bits: horizontal, lowest 4 bits: vertical
			  << (id == 1 ? 0x22 : 0x11) // 0x22 stands for YCbCr 4:2:0
			  << (id == 1 ? 0 : 1); // use quantization table 0 for Y, table 1 for Cb and Cr

}

void JPEGBitStreamWriter::writeHuffmanTables() {
	
          const auto numComponents = 3;
  // ////////////////////////////////////////
  // Huffman tables
  // DHT marker - define Huffman tables
  bitWriter.addMarker(0xC4, 2+208+208);
							// 2 bytes for the length field, store chrominance only if needed
							//   1+16+12  for the DC luminance
							//   1+16+162 for the AC luminance   (208 = 1+16+12 + 1+16+162)
							//   1+16+12  for the DC chrominance
							//   1+16+162 for the AC chrominance (208 = 1+16+12 + 1+16+162, same as above)

  // store luminance's DC+AC Huffman table definitions
  bitWriter << 0x00 // highest 4 bits: 0 => DC, lowest 4 bits: 0 => Y (baseline)
			<< DcLuminanceCodesPerBitsize
			<< DcLuminanceValues;
  bitWriter << 0x10 // highest 4 bits: 1 => AC, lowest 4 bits: 0 => Y (baseline)
			<< AcLuminanceCodesPerBitsize
			<< AcLuminanceValues;

  // compute actual Huffman code tables (see Jon's code for precalculated tables)
  generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, huffmanLuminanceDC);
  generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, huffmanLuminanceAC);

	// store luminance's DC+AC Huffman table definitions
	bitWriter << 0x01 // highest 4 bits: 0 => DC, lowest 4 bits: 1 => Cr,Cb (baseline)
			  << DcChrominanceCodesPerBitsize
			  << DcChrominanceValues;
	bitWriter << 0x11 // highest 4 bits: 1 => AC, lowest 4 bits: 1 => Cr,Cb (baseline)
			  << AcChrominanceCodesPerBitsize
			  << AcChrominanceValues;

	// compute actual Huffman code tables (see Jon's code for precalculated tables)
	generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, huffmanChrominanceDC);
	generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, huffmanChrominanceAC);

  // ////////////////////////////////////////
  // start of scan (there is only a single scan for baseline JPEGs)
  bitWriter.addMarker(0xDA, 2+1+2*numComponents+3); // 2 bytes for the length field, 1 byte for number of components,
													// then 2 bytes for each component and 3 bytes for spectral selection

  // assign Huffman tables to each component
  bitWriter << numComponents;
  for (auto id = 1; id <= numComponents; id++)
	// highest 4 bits: DC Huffman table, lowest 4 bits: AC Huffman table
	bitWriter << id << (id == 1 ? 0x00 : 0x11); // Y: tables 0 for DC and AC; Cb + Cr: tables 1 for DC and AC

  // constant values for our baseline JPEGs (which have a single sequential scan)
  static const uint8_t Spectral[3] = { 0, 63, 0 }; // spectral selection: must be from 0 to 63; successive approximation must be 0
  bitWriter << Spectral;


  // ////////////////////////////////////////
  // precompute JPEG codewords for quantized DCT
  codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
  uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
  int32_t mask    = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
  for (int16_t value = 1; value < CodeWordLimit; value++)
  {
	// numBits = position of highest set bit (ignoring the sign)
	// mask    = (2^numBits) - 1
	if (value > mask) // one more bit ?
	{
	  numBits++;
	  mask = (mask << 1) | 1; // append a set bit
	}
	codewords[-value] = BitCode(mask - value, numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
	codewords[+value] = BitCode(       value, numBits);
  }

}


void JPEGBitStreamWriter::writeBlockY(
	int16_t quantized[8*8]
) {
	lastYDC = writeBlock(quantized, lastYDC, huffmanLuminanceDC, huffmanLuminanceAC);
}

void JPEGBitStreamWriter::writeBlockU(
	int16_t quantized[8*8]
) {
	lastCbDC = writeBlock(quantized, lastCbDC, huffmanChrominanceDC, huffmanChrominanceAC);
}

void JPEGBitStreamWriter::writeBlockV(
	int16_t quantized[8*8]
) {
	lastCrDC = writeBlock(quantized, lastCrDC, huffmanChrominanceDC, huffmanChrominanceAC);
}


void JPEGBitStreamWriter::finishStream() {
	bitWriter.flush(); // now image is completely encoded, write any bits still left in the buffer

	// ///////////////////////////
	// EOI marker
	bitWriter << 0xFF << 0xD9; // this marker has no length, therefore I can't use addMarker()

	bitWriter.close();
}


void JPEGBitStreamWriter::generateHuffmanTable(const uint8_t numCodes[16], const uint8_t* values, BitCode result[256]) {
	// process all bitsizes 1 thru 16, no JPEG Huffman code is allowed to exceed 16 bits
	auto huffmanCode = 0;
	for(auto numBits = 1; numBits <= 16; numBits++){
		// ... and each code of these bitsizes
		for (auto i = 0; i < numCodes[numBits - 1]; i++){ // note: numCodes array starts at zero, but smallest bitsize is 1
			result[*values++] = BitCode(huffmanCode++, numBits);
		}

		// next Huffman code needs to be one bit wider
		huffmanCode <<= 1;
	}
}

int16_t JPEGBitStreamWriter::writeBlock(
	int16_t quantized[8*8],
	int16_t lastDC,
	const BitCode huffmanDC[256],
	const BitCode huffmanAC[256]
) {


  auto posNonZero = 0; // find last coefficient which is not zero (because trailing zeros are encoded differently)
  for (auto i = 0; i < 8*8; i++)
  {
	if (quantized[i] != 0)
	  posNonZero = i;
  }

  // encode DC (the first coefficient is the "average color" of the 8x8 block)
  auto DC = quantized[0];

  // same "average color" as previous block ?
  auto diff = DC - lastDC;
  if (diff == 0)
	bitWriter << huffmanDC[0x00];   // yes, write a special short symbol
  else
  {
	auto bits = codewords[diff]; // nope, encode the difference to previous block's average color
	bitWriter << huffmanDC[bits.numBits] << bits;
  }

  // encode ACs (quantized[1..63])
  auto offset = 0; // upper 4 bits count the number of consecutive zeros
  for (auto i = 1; i <= posNonZero; i++) // quantized[0] was already written, skip all trailing zeros, too
  {
	// zeros are encoded in a special way
	while (quantized[i] == 0) // found another zero ?
	{
	  offset    += 0x10; // add 1 to the upper 4 bits
	  // split into blocks of at most 16 consecutive zeros
	  if (offset > 0xF0) // remember, the counter is in the upper 4 bits, 0xF = 15
	  {
		bitWriter << huffmanAC[0xF0]; // 0xF0 is a special code for "16 zeros"
		offset = 0;
	  }
	  i++;
	}

	auto encoded = codewords[quantized[i]];
	// combine number of zeros with the number of bits of the next non-zero value
	bitWriter << huffmanAC[offset + encoded.numBits] << encoded; // and the value itself
	offset = 0;
  }

  // send end-of-block code (0x00), only needed if there are trailing zeros
  if (posNonZero < 8*8 - 1) // = 63
	bitWriter << huffmanAC[0x00];

  return DC;
}

