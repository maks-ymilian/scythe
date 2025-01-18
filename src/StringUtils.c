#include "StringUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int IntCharCount(int number)
{
	int minusSign = 0;
	if (number < 0)
	{
		number = abs(number);
		minusSign = 1;
	}
	if (number < 10) return 1 + minusSign;
	if (number < 100) return 2 + minusSign;
	if (number < 1000) return 3 + minusSign;
	if (number < 10000) return 4 + minusSign;
	if (number < 100000) return 5 + minusSign;
	if (number < 1000000) return 6 + minusSign;
	if (number < 10000000) return 7 + minusSign;
	if (number < 100000000) return 8 + minusSign;
	if (number < 1000000000) return 9 + minusSign;
	return 10 + minusSign;
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
	const size_t insertLength = (size_t)(IntCharCount(insert1) + IntCharCount(insert2));
	const size_t formatLength = strlen(format) - 4;
	const size_t bufferLength = insertLength + formatLength + 1;
	char* str = malloc(bufferLength);
	snprintf(str, bufferLength, format, insert1, insert2);
	return str;
}
