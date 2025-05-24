#include "StringUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t SignedIntCharCount(int64_t number)
{
	if (number < 0)
	{
		if (number > -10LL) return 2;
		if (number > -100LL) return 3;
		if (number > -1000LL) return 4;
		if (number > -10000LL) return 5;
		if (number > -100000LL) return 6;
		if (number > -1000000LL) return 7;
		if (number > -10000000LL) return 8;
		if (number > -100000000LL) return 9;
		if (number > -1000000000LL) return 10;
		if (number > -10000000000LL) return 11;
		if (number > -100000000000LL) return 12;
		if (number > -1000000000000LL) return 13;
		if (number > -10000000000000LL) return 14;
		if (number > -100000000000000LL) return 15;
		if (number > -1000000000000000LL) return 16;
		if (number > -10000000000000000LL) return 17;
		if (number > -100000000000000000LL) return 18;
		if (number > -1000000000000000000LL) return 19;
		return 20;
	}
	else
	{
		if (number < 10LL) return 1;
		if (number < 100LL) return 2;
		if (number < 1000LL) return 3;
		if (number < 10000LL) return 4;
		if (number < 100000LL) return 5;
		if (number < 1000000LL) return 6;
		if (number < 10000000LL) return 7;
		if (number < 100000000LL) return 8;
		if (number < 1000000000LL) return 9;
		if (number < 10000000000LL) return 10;
		if (number < 100000000000LL) return 11;
		if (number < 1000000000000LL) return 12;
		if (number < 10000000000000LL) return 13;
		if (number < 100000000000000LL) return 14;
		if (number < 1000000000000000LL) return 15;
		if (number < 10000000000000000LL) return 16;
		if (number < 100000000000000000LL) return 17;
		if (number < 1000000000000000000LL) return 18;
		return 19;
	}
}

size_t UnsignedIntCharCount(uint64_t number)
{
	if (number < 10ULL) return 1;
	if (number < 100ULL) return 2;
	if (number < 1000ULL) return 3;
	if (number < 10000ULL) return 4;
	if (number < 100000ULL) return 5;
	if (number < 1000000ULL) return 6;
	if (number < 10000000ULL) return 7;
	if (number < 100000000ULL) return 8;
	if (number < 1000000000ULL) return 9;
	if (number < 10000000000ULL) return 10;
	if (number < 100000000000ULL) return 11;
	if (number < 1000000000000ULL) return 12;
	if (number < 10000000000000ULL) return 13;
	if (number < 100000000000000ULL) return 14;
	if (number < 1000000000000000ULL) return 15;
	if (number < 10000000000000000ULL) return 16;
	if (number < 100000000000000000ULL) return 17;
	if (number < 1000000000000000000ULL) return 18;
	if (number < 10000000000000000000ULL) return 19;
	return 20;
}

char* AllocateString(const char* string)
{
	if (string == NULL) return NULL;
	const size_t length = strlen(string) + 1;
	char* new = malloc(length);
	memcpy(new, string, length);
	return new;
}

char* AllocateString1Str(const char* format, const char* insert)
{
	const size_t insertLength = strlen(insert);
	const size_t formatLength = strlen(format) - 2;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert);
	return str;
}

char* AllocateString2Str(const char* format, const char* insert1, const char* insert2)
{
	const size_t insertLength = strlen(insert1) + strlen(insert2);
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}

char* AllocateString3Str(const char* format, const char* insert1, const char* insert2, const char* insert3)
{
	const size_t insertLength = strlen(insert1) + strlen(insert2) + strlen(insert3);
	const size_t formatLength = strlen(format) - 6;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2, insert3);
	return str;
}

char* AllocateString2Int(const char* format, const int insert1, const int insert2)
{
	const size_t insertLength = SignedIntCharCount(insert1) + SignedIntCharCount(insert2);
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}

char* AllocateString1Str1Int(const char* format, const char* insert1, const int insert2)
{
	const size_t insertLength = strlen(insert1) + SignedIntCharCount(insert2);
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}
