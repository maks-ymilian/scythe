#pragma once

#include <stdio.h>
#include <stdlib.h>

#define UNREACHABLE() \
	do \
	{ \
		printf("Unreachable code in %s:%u\n", __FILE__, __LINE__); \
		exit(1);                                                      \
	} \
	while (0)

#define INVALID_VALUE(value)                                          \
	do                                                                \
	{                                                                 \
		const int v = (int)value;                                     \
		printf("Invalid value: %d\n", v); \
		UNREACHABLE(); \
	}                                                                 \
	while (0)
