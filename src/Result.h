#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "Token.h"

#define SUCCESS_RESULT (Result){true, false, NULL, 0}
#define ERROR_RESULT(message, lineNumber) (Result){false, true, message, lineNumber}
#define ERROR_RESULT_TOKEN(message, lineNumber, tokenType) (Result){false, true, message, lineNumber, tokenType};

#define HANDLE_ERROR(function)\
do{\
    const Result _res = function;\
    if (_res.hasError)\
        return _res;\
}\
while(0)

typedef struct
{
    bool success;
    bool hasError;
    const char* errorMessage;
    int lineNumber;
    TokenType tokenType;
} Result;

void PrintError(Result result);
