#pragma once

#include <inttypes.h>
#include <stddef.h>

// clang-format off

#define INT64_CHAR_COUNT(x) (\
	(((int64_t)x) < 0) ?\
	(\
		(((int64_t)x) > -10LL) ? ((size_t)2) : (\
		(((int64_t)x) > -100LL) ? ((size_t)3) : (\
		(((int64_t)x) > -1000LL) ? ((size_t)4) : (\
		(((int64_t)x) > -10000LL) ? ((size_t)5) : (\
		(((int64_t)x) > -100000LL) ? ((size_t)6) : (\
		(((int64_t)x) > -1000000LL) ? ((size_t)7) : (\
		(((int64_t)x) > -10000000LL) ? ((size_t)8) : (\
		(((int64_t)x) > -100000000LL) ? ((size_t)9) : (\
		(((int64_t)x) > -1000000000LL) ? ((size_t)10) : (\
		(((int64_t)x) > -10000000000LL) ? ((size_t)11) : (\
		(((int64_t)x) > -100000000000LL) ? ((size_t)12) : (\
		(((int64_t)x) > -1000000000000LL) ? ((size_t)13) : (\
		(((int64_t)x) > -10000000000000LL) ? ((size_t)14) : (\
		(((int64_t)x) > -100000000000000LL) ? ((size_t)15) : (\
		(((int64_t)x) > -1000000000000000LL) ? ((size_t)16) : (\
		(((int64_t)x) > -10000000000000000LL) ? ((size_t)17) : (\
		(((int64_t)x) > -100000000000000000LL) ? ((size_t)18) : (\
		(((int64_t)x) > -1000000000000000000LL) ? ((size_t)19) : (\
		((size_t)20)))))))))))))))))))\
	)\
	:\
	(\
		(((int64_t)x) < 10LL) ? ((size_t)1) : (\
		(((int64_t)x) < 100LL) ? ((size_t)2) : (\
		(((int64_t)x) < 1000LL) ? ((size_t)3) : (\
		(((int64_t)x) < 10000LL) ? ((size_t)4) : (\
		(((int64_t)x) < 100000LL) ? ((size_t)5) : (\
		(((int64_t)x) < 1000000LL) ? ((size_t)6) : (\
		(((int64_t)x) < 10000000LL) ? ((size_t)7) : (\
		(((int64_t)x) < 100000000LL) ? ((size_t)8) : (\
		(((int64_t)x) < 1000000000LL) ? ((size_t)9) : (\
		(((int64_t)x) < 10000000000LL) ? ((size_t)10) : (\
		(((int64_t)x) < 100000000000LL) ? ((size_t)11) : (\
		(((int64_t)x) < 1000000000000LL) ? ((size_t)12) : (\
		(((int64_t)x) < 10000000000000LL) ? ((size_t)13) : (\
		(((int64_t)x) < 100000000000000LL) ? ((size_t)14) : (\
		(((int64_t)x) < 1000000000000000LL) ? ((size_t)15) : (\
		(((int64_t)x) < 10000000000000000LL) ? ((size_t)16) : (\
		(((int64_t)x) < 100000000000000000LL) ? ((size_t)17) : (\
		(((int64_t)x) < 1000000000000000000LL) ? ((size_t)18) : (\
		((size_t)19)))))))))))))))))))\
	))

#define UINT64_CHAR_COUNT(x) (\
	(((uint64_t)x) < 10ULL) ? ((size_t)1) : (\
	(((uint64_t)x) < 100ULL) ? ((size_t)2) : (\
	(((uint64_t)x) < 1000ULL) ? ((size_t)3) : (\
	(((uint64_t)x) < 10000ULL) ? ((size_t)4) : (\
	(((uint64_t)x) < 100000ULL) ? ((size_t)5) : (\
	(((uint64_t)x) < 1000000ULL) ? ((size_t)6) : (\
	(((uint64_t)x) < 10000000ULL) ? ((size_t)7) : (\
	(((uint64_t)x) < 100000000ULL) ? ((size_t)8) : (\
	(((uint64_t)x) < 1000000000ULL) ? ((size_t)9) : (\
	(((uint64_t)x) < 10000000000ULL) ? ((size_t)10) : (\
	(((uint64_t)x) < 100000000000ULL) ? ((size_t)11) : (\
	(((uint64_t)x) < 1000000000000ULL) ? ((size_t)12) : (\
	(((uint64_t)x) < 10000000000000ULL) ? ((size_t)13) : (\
	(((uint64_t)x) < 100000000000000ULL) ? ((size_t)14) : (\
	(((uint64_t)x) < 1000000000000000ULL) ? ((size_t)15) : (\
	(((uint64_t)x) < 10000000000000000ULL) ? ((size_t)16) : (\
	(((uint64_t)x) < 100000000000000000ULL) ? ((size_t)17) : (\
	(((uint64_t)x) < 1000000000000000000ULL) ? ((size_t)18) : (\
	(((uint64_t)x) < 10000000000000000000ULL) ? ((size_t)19) : (\
	((size_t)20)))))))))))))))))))))

// clang-format on

#define INT64_MAX_CHARS (INT64_CHAR_COUNT(INT64_MAX))
#define UINT64_MAX_CHARS (UINT64_CHAR_COUNT(UINT64_MAX))

char* AllocUInt64ToString(uint64_t integer);
char* AllocSizeToString(size_t integer);

char* AllocateString(const char* string);
char* AllocateStringLength(const char* string, size_t length);
char* AllocateString1Str(const char* format, const char* insert);
char* AllocateString2Str(const char* format, const char* insert1, const char* insert2);
char* AllocateString3Str(const char* format, const char* insert1, const char* insert2, const char* insert3);
char* AllocateString2Int(const char* format, int64_t insert1, int64_t insert2);
char* AllocateString1Str1Int(const char* format, const char* insert1, int64_t insert2);
