#pragma once

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "Data.h"
#include "Result.h"

#define HANDLE_ERROR(function)\
{const Result result = function;\
if (result.hasError)\
    return result;}

#define ASSERT_ERROR(function)\
{const Result result = function;\
if (result.hasError){\
    printf("Assertion failed: %s\n", result.errorMessage);\
    assert(0);}}

#define WRITE_LITERAL(text) Write(text, sizeof(text) - 1)
#define WRITE_TEXT(text) Write(text, strlen(text));

static inline int CountCharsInNumber(long number)
{
    if (number == 0)
        return 1;

    int minus = 0;
    if (number < 0)
    {
        number = -number;
        minus = 1;
    }

    if (number < 10)
        return 1 + minus;
    if (number < 100)
        return 2 + minus;
    if (number < 1000)
        return 3 + minus;
    if (number < 10000)
        return 4 + minus;
    if (number < 100000)
        return 5 + minus;
    if (number < 1000000)
        return 6 + minus;
    if (number < 10000000)
        return 7 + minus;
    if (number < 100000000)
        return 8 + minus;
    if (number < 1000000000)
        return 9 + minus;
    return 10 + minus;
}

static inline void WriteInteger(const long integer)
{
    char string[CountCharsInNumber(integer) + 1];
    snprintf(string, sizeof(string), "%ld", integer);
    Write(string, sizeof(string) - 1);
}