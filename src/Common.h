#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "PlatformUtils.h"

#define UNREACHABLE()                                                       \
	do                                                                      \
	{                                                                       \
		fprintf(stderr, "Assertion failed in %s:%u\n", __FILE__, __LINE__); \
		PrintStackTrace();                                                  \
		abort();                                                            \
	}                                                                       \
	while (0)

#define ASSERT(x)          \
	do                     \
	{                      \
		if (!(x))          \
			UNREACHABLE(); \
	}                      \
	while (0)

#define INVALID_VALUE(value)                            \
	do                                                  \
	{                                                   \
		int value_ = (int)(value);                      \
		fprintf(stderr, "Invalid value: %d\n", value_); \
		UNREACHABLE();                                  \
	}                                                   \
	while (0)

#define TODO()                     \
	do                             \
	{                              \
		fprintf(stderr, "todo\n"); \
		UNREACHABLE();             \
	}                              \
	while (0)
