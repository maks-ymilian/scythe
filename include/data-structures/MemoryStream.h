#pragma once

#include <stdbool.h>

#include "inttypes.h"

typedef struct MemoryStream MemoryStream;

MemoryStream* AllocateMemoryStream();
uint8_t* FreeMemoryStream(MemoryStream* stream, bool freeBuffer);

void StreamWrite(MemoryStream* stream, const void* buffer, size_t length);
uint8_t* StreamGetBuffer(const MemoryStream* stream);