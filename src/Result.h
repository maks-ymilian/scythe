#pragma once

#include <assert.h>
#include <stdio.h>

#include "Token.h"

#define SUCCESS_RESULT          \
	(Result)                    \
	{                           \
		.type = Result_Success, \
		.errorMessage = NULL,   \
		.lineNumber = -1,       \
		.filePath = NULL,       \
	}

#define ERROR_RESULT(message, line, path) \
	(Result)                              \
	{                                     \
		.type = Result_Error,             \
		.errorMessage = message,          \
		.lineNumber = line,               \
		.filePath = path,                 \
	}

#define NOT_FOUND_RESULT         \
	(Result)                     \
	{                            \
		.type = Result_NotFound, \
		.errorMessage = NULL,    \
		.lineNumber = -1,        \
		.filePath = NULL,        \
	}

#define PROPAGATE_ERROR(function)                        \
	do                                                   \
	{                                                    \
		const Result _propagateErrorResult = (function); \
		if (_propagateErrorResult.type == Result_Error)  \
			return _propagateErrorResult;                \
	}                                                    \
	while (0)

#define PROPAGATE_FOUND(function)                          \
	do                                                     \
	{                                                      \
		const Result _propagateFoundResult = (function);   \
		if (_propagateFoundResult.type != Result_NotFound) \
			return _propagateFoundResult;                  \
	}                                                      \
	while (0)

typedef enum
{
	Result_Success,
	Result_Error,
	Result_NotFound,
} ResultType;

typedef struct
{
	ResultType type;
	int lineNumber;
	const char* errorMessage;
	const char* filePath;
} Result;

static void PrintError(const Result result)
{
	assert(result.type == Result_Error);
	assert(result.errorMessage != NULL);
	printf("%s", result.errorMessage);
	if (result.lineNumber > 0)
		printf(" (line %d)", result.lineNumber);
	if (result.filePath != NULL)
		printf(" (%s)", result.filePath);
}
