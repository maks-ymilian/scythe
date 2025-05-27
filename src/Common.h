#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define UNREACHABLE()                                                       \
	do                                                                      \
	{                                                                       \
		fprintf(stderr, "Unreachable code in %s:%u\n", __FILE__, __LINE__); \
		exit(EXIT_FAILURE);                                                 \
	}                                                                       \
	while (0)

#define INVALID_VALUE(value)                       \
	do                                             \
	{                                              \
		const int v = (int)value;                  \
		fprintf(stderr, "Invalid value: %d\n", v); \
		UNREACHABLE();                             \
	}                                              \
	while (0)

#define TODO()                     \
	do                             \
	{                              \
		fprintf(stderr, "todo\n"); \
		exit(EXIT_FAILURE);        \
	}                              \
	while (0)
