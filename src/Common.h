#pragma once

#include <stddef.h>
#include <stdio.h>

#define INVALID_VALUE(value)                                          \
	do                                                                \
	{                                                                 \
		const int v = (int)value;                                     \
		printf("Invalid value %d in %s:%u\n", v, __FILE__, __LINE__); \
		unreachable();                                                \
	}                                                                 \
	while (0)
