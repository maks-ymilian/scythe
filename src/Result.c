#include "Result.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <data-structures/MemoryStream.h>

void PrintError(const Result result)
{
	assert(result.errorMessage != NULL);

	MemoryStream* stream = AllocateMemoryStream();

	size_t i = 0;
	while (1)
	{
		if (result.errorMessage[i] == '#' &&
			result.errorMessage[i + 1] == 't')
		{
			i += 2;

			const char* tokenName = GetTokenTypeString(result.tokenType);
			StreamWrite(stream, tokenName, strlen(tokenName));
		}

		const char current = result.errorMessage[i];

		StreamWriteByte(stream, current);

		if (current == '\0')
			break;

		i++;
	}

	const char* buffer = (char*)StreamGetBuffer(stream).buffer;
	if (result.lineNumber > 0)
		printf("%s (line %d)", buffer, result.lineNumber);\
	else
		printf("%s", buffer);\

	FreeMemoryStream(stream, true);
}
