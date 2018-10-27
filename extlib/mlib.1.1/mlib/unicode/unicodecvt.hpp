#pragma once
#include <string>
#include <stdexcept>
#include "../platform.hpp"

//
// unicode conversion routines
//

namespace mlib{namespace unicode
{

struct ConversionError : public std::logic_error
{
	ConversionError(const std::string &msg) : logic_error("unicode" + msg) {}
};

struct InvalidCodepoint : public ConversionError
{
	InvalidCodepoint() : ConversionError(".invcodepoint") {}
};

//
// utf8
//

namespace utf8
{

//
// generic decode code point
//	

template<class UnitExtractor>
char32_t decodeBase(UnitExtractor extract)
{
	char32_t o1 = (char32_t)(unsigned char)extract();
	if((o1 >> 6) == 0b11)
	{
		char32_t o2 = (char32_t)(unsigned char)extract();
		if((o1 >> 5) == 0b111)
		{
			char32_t o3 = (char32_t)(unsigned char)extract();
			if((o1 >> 4) == 0b1111)
			{
				char32_t o4 = (char32_t)(unsigned char)extract();
				return ((o1 & 0b111) << 18) | ((o2 & 0b111111) << 12) | ((o3 & 0b111111) << 6) | (o4 & 0b111111);
			}
			return ((o1 & 0b1111) << 12) | ((o2 & 0b111111) << 6) | (o3 & 0b111111);
		}
		return ((o1 & 0b11111) << 6) | (o2 & 0b111111);
	}
	return o1;
}

//
// decode from iterator
//

template<class Iter>
char32_t decode(Iter &i)
{
	return decodeBase([&]() { return *i++ & 0xFF; });
}

//
//decode from iterator range
//

template<class Iter>
char32_t decode(Iter &i, Iter end)
{
	return decodeBase([&]()
	{
		if(i != end)
			return (*i++) & 0xFF;
		else
			throw InvalidCodepoint();
	});
}

//
// search previous codepoint
//

inline bool isStartbyte(char c) //either high bit/bits is/are 0 (compatible ascii byte) or 11 (start byte)
{
    unsigned int v = (unsigned int)(unsigned char)c;
    auto r = (v >> 7 & 1) == 0 || (v >> 5 & 0b111) == 0b110 || (v >> 4 & 0b1111) == 0b1110 || (v >> 3 & 0b11111) == 0b11110;
    return r;
}

template<class ReverseUnitExtractor>
void previous(ReverseUnitExtractor extract)
{
    char unit;
    do unit = extract(); while(!isStartbyte(unit));
}

template<class Iter>
Iter previous(Iter iter, Iter begin)
{
    previous([&]()
    {
        if(iter == begin)
            throw InvalidCodepoint();
        return *--iter;
    });
    return iter;
}

//
// generic encode codepoint
//

template<class UnitInserter>
void encodeBase(UnitInserter insert, char32_t cp)
{
	if(cp < 0x80)
	{
		insert((char)(unsigned char)cp);
	}
	else if(cp < 0x800)
	{
		insert((char)(unsigned char)((cp >> 6) | 0xC0));
		insert((char)(unsigned char)((cp & 0x3F) | 0x80));
	}
	else if(cp < 0x10000) 
	{
		insert((char)(unsigned char)((cp >> 12) | 0xe0));
		insert((char)(unsigned char)(((cp >> 6) & 0x3F) | 0x80));
		insert((char)(unsigned char)((cp & 0x3f) | 0x80));
	}
	else 
	{
		insert((char)(unsigned char)((cp >> 18) | 0xF0));
		insert((char)(unsigned char)(((cp >> 12) & 0x3F) | 0x80));
		insert((char)(unsigned char)(((cp >> 6) & 0x3F) | 0x80));
		insert((char)(unsigned char)((cp & 0x3f) | 0x80));
	}
}

//
// encode code point to iterator
//

template<class Iter>
void encode(Iter &i, char32_t cp)
{
	encodeBase([&](char u) { *i++ = u; }, cp);
}

}

//
// utf16
//

namespace utf16
{

//
// generic decode code point
//

template<class UnitExtractor>
char32_t decodeBase(UnitExtractor extract)
{
	char32_t s1 = extract();
	if(s1 >= 0xD800 && s1 <= 0xDBFF)
	{
		char32_t s2 = (char32_t)extract();
		if(s2 >= 0xDC00 && s2 <= 0xDFFF)
			return (((s1 - 0xD800) << 10) | (s2 - 0xDC00)) + 0x10000;
		else
			return s1;
	}
	return s1;
}

//
// decode code point from iterator
//

template<class Iter>
char32_t decode(Iter &i)
{
	return decodeBase([&]() { return *i++ & 0xFFFF; });
}

//
//decode code point from iterator range
//

template<class Iter>
char32_t decode(Iter &i, Iter end)
{
	return decodeBase([&]()
	{
		if(i != end)
			return *i++ & 0xFFFF;
		else
			throw InvalidCodepoint();
	});
}

//
// generic encode codepoint
//

template<class UnitInserter>
void encodeBase(UnitInserter insert, char32_t c)
{
	if(c >= 0x10000)
	{
		char16_t s = (char16_t)(c - 0x10000);
		insert((s >> 10) + 0xD800);
		insert((s & 0x3FF) + 0xDC00);
	}
	else
	{
		insert((char16_t)(c & 0xFFFF));
		if(c >= 0xD800 && c <= 0xDBFF)
			insert(1);
	}
}

//
// encode codepoint to iterator
//

template<class Iter>
void encode(Iter &i, char32_t cp)
{
	EncodeBase([&](char16_t u) { *i++ = u; }, cp);
}

}

//
// native wchar_t encoding
//

namespace native
{

template<class UnitExtractor>
char32_t decodeBase(UnitExtractor extract)
{
#ifdef MLIB_UNICODE_UTF16
	return utf16::decodeBase(extract);
#else
	return extract();
#endif
}

template<class UnitInserter>
void encodeBase(UnitInserter insert, char32_t cp)
{
#ifdef MLIB_UNICODE_UTF16
	utf16::encodeBase(insert, cp);
#else
	insert(cp);
#endif
}

}

//
// utilities to convert from native to utf8
//

std::string fromNative(const wchar_t *wstr);
std::string fromNative(const std::wstring &wstr);
std::wstring toNative(const char *str);
std::wstring toNative(const std::string &s);

}}