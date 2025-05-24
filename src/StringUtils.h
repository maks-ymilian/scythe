#pragma once

#include <inttypes.h>

#define INT64_MAX_CHARS 20

size_t SignedIntCharCount(int64_t number);
size_t UnsignedIntCharCount(uint64_t number);

char* AllocateString(const char* string);
char* AllocateString1Str(const char* format, const char* insert);
char* AllocateString2Str(const char* format, const char* insert1, const char* insert2);
char* AllocateString3Str(const char* format, const char* insert1, const char* insert2, const char* insert3);
char* AllocateString2Int(const char* format, int insert1, int insert2);
char* AllocateString1Str1Int(const char* format, const char* insert1, const int insert2);
