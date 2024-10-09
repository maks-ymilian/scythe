#pragma once

#include <stdbool.h>

#include "inttypes.h"

typedef struct MemoryStream MemoryStream;

typedef struct
{
    uint8_t* buffer;
    size_t length;
}Buffer;

MemoryStream* AllocateMemoryStream();
void FreeMemoryStream(MemoryStream* stream, bool freeBuffer);

size_t StreamWrite(MemoryStream* stream, const void* buffer, size_t length);
void StreamWriteByte(MemoryStream* stream, uint8_t data);

Buffer StreamRead(MemoryStream* stream, size_t length);
Buffer StreamRewindRead(MemoryStream* stream, size_t offset);
Buffer StreamGetBuffer(const MemoryStream* stream);

void StreamRewind(MemoryStream* stream, size_t offset);

size_t StreamGetPosition(const MemoryStream* stream);
void StreamSetPosition(MemoryStream* stream, size_t position);