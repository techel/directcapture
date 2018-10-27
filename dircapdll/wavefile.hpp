#pragma once

#include <iosfwd>
#include <cstddef>

class WaveFile
{
public:
	WaveFile() = default;
	~WaveFile();

	void open(std::ostream &o, long samplerate, unsigned int bitspersample, unsigned int channels);
	void close();

	void write(const char *data, size_t len);

private:
	std::streamsize TotalLength;
	std::ostream *Stream = nullptr;
};