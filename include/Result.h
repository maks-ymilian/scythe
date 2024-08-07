#pragma once

#include <stdbool.h>
#include "Token.h"

#define SUCCESS_RESULT (Result){true, false, NULL, 0}
#define ERROR_RESULT(message, lineNumber) (Result){false, true, message, lineNumber}

typedef struct
{
	bool success;
	bool hasError;
	const char* errorMessage;
	int lineNumber;
	TokenType tokenType;
}Result;

void PrintError(Result result);