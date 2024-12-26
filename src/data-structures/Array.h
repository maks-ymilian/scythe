#pragma once

#include <inttypes.h>

typedef struct
{
	void** array;
	size_t length, cap, sizeOfType;
} Array;

Array AllocateArray(size_t sizeOfType);
void ArrayAdd(Array* array, const void* item);
void ArrayInsert(Array* array, const void* item, size_t index);
void ArrayRemove(Array* array, size_t index);
void ArrayClear(Array* array);
void FreeArray(const Array* array);
