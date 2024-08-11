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
Buffer FreeMemoryStream(MemoryStream* stream, bool freeBuffer);

size_t StreamWrite(MemoryStream* stream, const void* buffer, size_t length);
void StreamWriteByte(MemoryStream* stream, uint8_t data);

Buffer StreamRead(MemoryStream* stream, size_t length);
Buffer StreamReadRest(MemoryStream* stream);

void StreamRewind(MemoryStream* stream, size_t offset);