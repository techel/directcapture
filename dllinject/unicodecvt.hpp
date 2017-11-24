#pragma once
#include <string>
#include <stdexcept>

#define YFW_UNICODE_UTF16

//
// unicode conversion routines
//

namespace YFW{namespace Unicode
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

namespace Utf8
{

//
// generic decode code point
//	

template<class UnitExtractor>
char32_t decodeBase(UnitExtractor extract)
{
	char32_t o1 = static_cast<char32_t>(extract());
	if((o1 >> 6) == 0b11)
	{
		char32_t o2 = static_cast<char32_t>(extract());
		if((o1 >> 5) == 0b111)
		{
			char32_t o3 = static_cast<char32_t>(extract());
			if((o1 >> 4) == 0b1111)
			{
				char32_t o4 = static_cast<char32_t>(extract());
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
    unsigned int v = static_cast<unsigned int>(c);
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
		insert(static_cast<char>(cp));
	}
	else if(cp < 0x800)
	{
		insert(static_cast<char>((cp >> 6) | 0xC0));
		insert(static_cast<char>((cp & 0x3F) | 0x80));
	}
	else if(cp < 0x10000) 
	{
		insert(static_cast<char>((cp >> 12) | 0xe0));
		insert(static_cast<char>(((cp >> 6) & 0x3F) | 0x80));
		insert(static_cast<char>((cp & 0x3f) | 0x80));
	}
	else 
	{
		insert(static_cast<char>((cp >> 18) | 0xF0));
		insert(static_cast<char>(((cp >> 12) & 0x3F) | 0x80));
		insert(static_cast<char>(((cp >> 6) & 0x3F) | 0x80));
		insert(static_cast<char>((cp & 0x3f) | 0x80));
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

namespace Utf16
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
		char32_t s2 = static_cast<char32_t>(extract());
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
		char16_t s = static_cast<char16_t>(c - 0x10000);
		insert((s >> 10) + 0xD800);
		insert((s & 0x3FF) + 0xDC00);
	}
	else
	{
		insert(static_cast<char16_t>(c & 0xFFFF));
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

namespace Native
{

template<class UnitExtractor>
char32_t decodeBase(UnitExtractor extract)
{
#ifdef YFW_UNICODE_UTF16
	return Utf16::decodeBase(extract);
#else
	return extract();
#endif
}

template<class UnitInserter>
void encodeBase(UnitInserter insert, char32_t cp)
{
#ifdef YFW_UNICODE_UTF16
	Utf16::encodeBase(insert, cp);
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