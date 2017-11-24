#include "unicodecvt.hpp"

namespace YFW{namespace Unicode
{

std::string fromNative(const wchar_t *wstr)
{
	std::string dst;
	while(*wstr != L'\0')
	{
		Utf8::encodeBase([&](char c)
		{
			dst += c;
		},
		Native::decodeBase([&]()
		{
			if(*wstr == L'\0')
				throw InvalidCodepoint();
			else
				return *wstr++;
		}));
	}
	return dst;
}
std::string fromNative(const std::wstring &wstr)
{
	return fromNative(wstr.c_str());
}
std::wstring toNative(const char *str)
{
	std::wstring dst;
	while(*str != '\0')
	{
		Native::encodeBase(
		[&](wchar_t c)
		{
			dst += c;
		},
		Utf8::decodeBase([&]()
		{
			if(*str == '\0')
				throw InvalidCodepoint();

			return *str++;
		}));
	}
	return dst;
}
std::wstring toNative(const std::string &s)
{
	return toNative(s.c_str());
}

}}