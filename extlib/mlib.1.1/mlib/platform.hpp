#pragma once

//
// platform detection
//

#if defined(_WIN32)
	#define MLIB_PLATFORM_WIN32
#elif defined(__APPLE__) && defined(__MACH__)
    #define MLIB_PLATFORM_UNIX
    #define MLIB_PLATFORM_APPLEOS
	#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		#define MLIB_PLATFORM_IOS
	#elif TARGET_OS_MAC
		#define MLIB_PLATFORM_MACOS
	#else
		#error unknown Apple operating system
	#endif
#elif defined(__unix__)
    #define MLIB_PLATFORM_UNIX
	#if defined(__ANDROID__)
		#define MLIB_PLATFORM_ANDROID
	#elif defined(__linux__)
		#define MLIB_PLATFORM_LINUX
	#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
		#define MLIB_PLATFORM_FREEBSD
	#else
		#error unknown unix operating system
	#endif
#else
	#error unknown operating system
#endif

#if defined MLIB_PLATFORM_WIN32
	#define MLIB_UNICODE_UTF16
#else
	#define MLIB_UNICODE_UTF8
#endif

//
// endianess detection
//

#ifdef MLIB_PLATFORM_WIN32
	#define MLIB_BYTEORDER_BE
#elif defined(__BYTE_ORDER__)
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		#define MLIB_BYTEORDER_BE
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		#define MLIB_BYTEORDER_LE
	#else
		#error unsupported endiannes
	#endif
#else
	#error unknown endianness
#endif
