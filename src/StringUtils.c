#include "StringUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"

char* AllocUInt64ToString(uint64_t integer)
{
	int numChars = snprintf(NULL, 0, "%" PRIu64, integer);
	ASSERT(numChars >= 1);
	char* string = malloc((size_t)numChars + 1);

	if (snprintf(string, (size_t)numChars + 1, "%" PRIu64, integer) != numChars)
		ASSERT(0);

	return string;
}

char* AllocSizeToString(size_t integer)
{
	int numChars = snprintf(NULL, 0, "%zu", integer);
	ASSERT(numChars >= 1);
	char* string = malloc((size_t)numChars + 1);

	if (snprintf(string, (size_t)numChars + 1, "%zu", integer) != numChars)
		ASSERT(0);

	return string;
}

char* AllocateString(const char* string)
{
	if (string == NULL) return NULL;
	const size_t length = strlen(string) + 1;
	char* new = malloc(length);
	memcpy(new, string, length);
	return new;
}

char* AllocateStringLength(const char* string, size_t length)
{
	if (string == NULL) return NULL;
	char* new = malloc(length + 1);
	memcpy(new, string, length);
	new[length] = '\0';
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

char* AllocateString2Int(const char* format, int64_t insert1, int64_t insert2)
{
	const size_t insertLength = INT64_CHAR_COUNT(insert1) + INT64_CHAR_COUNT(insert2);
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}

char* AllocateString1Str1Int(const char* format, const char* insert1, int64_t insert2)
{
	const size_t insertLength = strlen(insert1) + INT64_CHAR_COUNT(insert2);
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}
