#include "wavefile.hpp"

#include <cassert>
#include <ostream>
#include <cstdint>

template<class T>
std::ostream &serialize(std::ostream &o, const T &t)
{
	return o.write((const char*)&t, sizeof(t));
}

WaveFile::~WaveFile()
{
	close();
}

void WaveFile::open(std::ostream &o, long samplerate, unsigned int bitspersample, unsigned int channels)
{
	close();

	Stream = &o;
	TotalLength = 0;

	o.write("RIFF", 4);
	serialize(o, uint32_t(0)); //offset 0x04: file size - 8, will be fixed when closing file

	o.write("WAVE", 4);
	o.write("fmt ", 4);
	serialize(o, uint32_t(16)); //fmt header length
	serialize(o, uint16_t(1)); //PCM format
	serialize(o, uint16_t(channels)); //channels
	serialize(o, uint32_t(samplerate)); //samplerate

	const auto framesize = channels * (bitspersample + 7) / 8;
	serialize(o, uint32_t(samplerate * framesize)); //bytes per second
	serialize(o, uint16_t(framesize)); //frame size
	serialize(o, uint16_t(bitspersample)); //bits per sample

	o.write("data", 4);
	serialize(o, uint32_t(0)); //offset 0x28: data size, will be fixed when closing file
}

void WaveFile::close()
{
	if(Stream)
	{
		Stream->seekp(0x04); //fix file size
		serialize(*Stream, static_cast<uint32_t>(TotalLength + 44 - 8)); //header size = 44

		Stream->seekp(0x28); //fix data size
		serialize(*Stream, static_cast<uint32_t>(TotalLength));

		Stream = nullptr;
	}
}

void WaveFile::write(const char *data, size_t len)
{
	assert(Stream);
	Stream->write(data, len);
	TotalLength += len;
}
