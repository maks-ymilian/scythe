#include "data-structures/Array.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START_SIZE 16

Array AllocateArray(const size_t sizeOfType)
{
	assert(sizeOfType > 0);

	Array array;
	array.array = malloc(START_SIZE * sizeof(void*));
	array.length = 0;
	array.cap = START_SIZE;
	array.sizeOfType = sizeOfType;
	return array;
}

void ArrayAdd(Array* array, const void* item)
{
	assert(item != NULL);
	assert(array->array != NULL);

	array->length++;
	if (array->length > array->cap)
	{
		array->cap *= 2;
		void* new = realloc(array->array, array->cap * sizeof(void*));
		assert(new != NULL);
		array->array = new;
	}

	void* ptr = malloc(array->sizeOfType);
	array->array[array->length - 1] = ptr;
	memcpy(ptr, item, array->sizeOfType);
}

void ArrayInsert(Array* array, const void* item, const size_t index)
{
	assert(item != NULL);
	assert(index <= array->length);

	array->length++;
	if (array->length > array->cap)
	{
		array->cap *= 2;
		void* new = realloc(array->array, array->cap * sizeof(void*));
		assert(new != NULL);
		array->array = new;
	}

	memmove(array->array + index + 1, array->array + index, (array->length - 1 - index) * sizeof(void*));

	void* ptr = malloc(array->sizeOfType);
	array->array[index] = ptr;
	memcpy(ptr, item, array->sizeOfType);
}

void ArrayRemove(Array* array, const size_t index)
{
	assert(index < array->length);
	free(array->array[index]);
	if (index != array->length - 1)
		memmove(array->array + index, array->array + index + 1, (array->length - index - 1) * sizeof(void*));
	array->length -= 1;
}

void FreeArray(const Array* array)
{
	for (size_t i = 0; i < array->length; ++i)
		free(array->array[i]);
	free(array->array);
}

void ArrayClear(Array* array)
{
	assert(array->array != NULL);
	for (size_t i = 0; i < array->length; ++i)
		free(array->array[i]);
	array->length = 0;
}
