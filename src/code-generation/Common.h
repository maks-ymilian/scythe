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