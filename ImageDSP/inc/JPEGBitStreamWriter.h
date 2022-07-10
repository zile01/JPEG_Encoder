
#ifndef JPEG_BIT_STREAMER_WRITER
#define JPEG_BIT_STREAMER_WRITER

#include <QString>
#include <fstream>




// restrict a value to the interval [minimum, maximum]
template <typename Number, typename Limit>
Number clamp(Number value, Limit minValue, Limit maxValue)
{
  if (value <= minValue) return minValue; // never smaller than the minimum
  if (value >= maxValue) return maxValue; // never bigger  than the maximum
  return value;                           // value was inside interval, keep it
}

const int16_t CodeWordLimit = 2048; // +/-2^11, maximum value after DCT

class JPEGBitStreamWriter {
	// represent a single Huffman code
	struct BitCode
	{
	  BitCode() = default; // undefined state, must be initialized at a later time
	  BitCode(uint16_t code_, uint8_t numBits_)
	  : code(code_), numBits(numBits_) {}
	  uint16_t code;       // JPEG's Huffman codes are limited to 16 bits
	  uint8_t  numBits;    // number of valid bits
	};

	// wrapper for bit output operations
	struct BitWriter
	{
	  std::ofstream _file;
	  // initialize writer
	  explicit BitWriter(const QString& out_fn)
	    : _file(out_fn.toStdString().c_str(), std::ios_base::out | std::ios_base::binary) {
	  }

	  void output(unsigned char byte){
	    _file << byte;
	  }

	  // store the most recently encoded bits that are not written yet
	  struct BitBuffer
	  {
	    int32_t data    = 0; // actually only at most 24 bits are used
	    uint8_t numBits = 0; // number of valid bits (the right-most bits)
	  } buffer;

	  // write Huffman bits stored in BitCode, keep excess bits in BitBuffer
	  BitWriter& operator<<(const BitCode& data)
	  {
	    // append the new bits to those bits leftover from previous call(s)
	    buffer.numBits += data.numBits;
	    buffer.data   <<= data.numBits;
	    buffer.data    |= data.code;

	    // write all "full" bytes
	    while (buffer.numBits >= 8)
	    {
	      // extract highest 8 bits
	      buffer.numBits -= 8;
	      auto oneByte = uint8_t(buffer.data >> buffer.numBits);
	      output(oneByte);

	      if (oneByte == 0xFF) // 0xFF has a special meaning for JPEGs (it's a block marker)
	        output(0);         // therefore pad a zero to indicate "nope, this one ain't a marker, it's just a coincidence"

	      // note: I don't clear those written bits, therefore buffer.bits may contain garbage in the high bits
	      //       if you really want to "clean up" (e.g. for debugging purposes) then uncomment the following line
	      //buffer.bits &= (1 << buffer.numBits) - 1;
	    }
	    return *this;
	  }

	  // write all non-yet-written bits, fill gaps with 1s (that's a strange JPEG thing)
	  void flush()
	  {
	    // at most seven set bits needed to "fill" the last byte: 0x7F = binary 0111 1111
	    *this << BitCode(0x7F, 7); // I should set buffer.numBits = 0 but since there are no single bits written after flush() I can safely ignore it
	  }
	  void close()
	  {
		_file.flush();
		_file.close();
	  }

	  // NOTE: all the following BitWriter functions IGNORE the BitBuffer and write straight to output !
	  // write a single byte
	  BitWriter& operator<<(uint8_t oneByte)
	  {
	    output(oneByte);
	    return *this;
	  }

	  // write an array of bytes
	  template <typename T, int Size>
	  BitWriter& operator<<(T (&manyBytes)[Size])
	  {
	    for (auto c : manyBytes)
	      output(c);
	    return *this;
	  }

	  // start a new JFIF block
	  void addMarker(uint8_t id, uint16_t length)
	  {
	    output(0xFF); output(id);     // ID, always preceded by 0xFF
	    output(uint8_t(length >> 8)); // length of the block (big-endian, includes the 2 length bytes as well)
	    output(uint8_t(length & 0xFF));
	  }
	};
	
	BitWriter bitWriter;
	
    BitCode  codewordsArray[2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
    BitCode* codewords;


    // average color of the previous MCU
    int16_t lastYDC;
    int16_t lastCbDC;
    int16_t lastCrDC;
    BitCode huffmanLuminanceDC[256];
    BitCode huffmanLuminanceAC[256];
    BitCode huffmanChrominanceDC[256];
    BitCode huffmanChrominanceAC[256];
public:
    JPEGBitStreamWriter(const QString& out_fn)
        : bitWriter(out_fn) {
		lastYDC = 0;
		lastCbDC = 0;
		lastCrDC = 0;
    }


public:
	void writeHeader();
	void writeQuantizationTables(uint8_t LuminanceQuantTable[64], uint8_t ChrominanceQuantTable[64]);
	void writeImageInfo(int width, int height);
	void writeHuffmanTables();
	void writeBlockY(int16_t quantized[8*8]);
	void writeBlockU(int16_t quantized[8*8]);
	void writeBlockV(int16_t quantized[8*8]);
	void finishStream();


private:
	void generateHuffmanTable(const uint8_t numCodes[16], const uint8_t* values, BitCode result[256]);
	
    int16_t writeBlock(
        int16_t quantized[8*8],
        int16_t lastDC,
        const BitCode huffmanDC[256],
        const BitCode huffmanAC[256]
    );

};

#endif // JPEG_BIT_STREAMER_WRITER
