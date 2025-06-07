#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct MemoryStream MemoryStream;

typedef struct
{
	char* buffer;
	size_t length;
} Buffer;

MemoryStream* AllocateMemoryStream(void);
void FreeMemoryStream(MemoryStream* stream, bool freeBuffer);

void StreamWrite(MemoryStream* stream, const void* buffer, size_t length);
void StreamWriteByte(MemoryStream* stream, char data);
void StreamWriteStream(MemoryStream* destination, MemoryStream* source);

Buffer StreamRead(MemoryStream* stream, size_t length);
Buffer StreamRewindRead(MemoryStream* stream, size_t offset);
Buffer StreamGetBuffer(const MemoryStream* stream);

void StreamRewind(MemoryStream* stream, size_t offset);

size_t StreamGetPosition(const MemoryStream* stream);
void StreamSetPosition(MemoryStream* stream, size_t position);
