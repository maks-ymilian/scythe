#include "Result.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "data-structures/Array.h"

static void AddString(Array* charArray, const char* string)
{
	size_t i = 0;
	while(1)
	{
		char current = string[i];

		if (current == '\0')
			break;

		ArrayAdd(charArray, &current);

		i++;
	}
}

void PrintError(const Result result)
{
	assert(result.errorMessage != NULL);

	Array output = AllocateArray(1, sizeof(char));

	size_t i = 0;
	while (1)
	{
		if (result.errorMessage[i] == '%' &&
			result.errorMessage[i + 1] == 't')
		{
			i += 2;
			AddString(&output, GetTokenTypeString(result.tokenType));
		}

		char current = result.errorMessage[i];

		ArrayAdd(&output, &current);

		if (current == '\0')
			break;

		i++;
	}

	char* buffer = malloc(output.length * sizeof(char));
	// ReSharper disable once CppDeclarationHidesLocal
	for (int i = 0; i < output.length; ++i)
		buffer[i] = *((char*)output.array[i]);
	FreeArray(&output);

	printf("%s (line %"PRIu32")\n", buffer, result.lineNumber);\
	free(buffer);
}
