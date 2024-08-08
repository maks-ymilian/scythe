#include "data-structures/MemoryStream.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define START_SIZE 256

typedef struct MemoryStream
{
    uint8_t* buffer;
    size_t position, capacity;
} MemoryStream;

MemoryStream* AllocateMemoryStream()
{
    MemoryStream* stream = malloc(sizeof(MemoryStream));
    stream->buffer = malloc(START_SIZE * sizeof(uint8_t));
    stream->capacity = START_SIZE;
    stream->position = 0;
    return stream;
}

uint8_t* FreeMemoryStream(MemoryStream* stream, const bool freeBuffer)
{
    uint8_t* buffer = stream->buffer;
    free(stream);

    if (freeBuffer)
    {
        free(buffer);
        return NULL;
    }

    return buffer;
}

static void Reallocate(MemoryStream* stream)
{
    stream->capacity *= 2;
    stream->buffer = realloc(stream->buffer, stream->capacity);
}

void StreamWrite(MemoryStream* stream, const void* buffer, const size_t length)
{
    assert(buffer != NULL);
    assert(length > 0);

    if (stream->position + length >= stream->capacity)
        Reallocate(stream);

    memcpy(stream->buffer + stream->position, buffer, length);
    stream->position += length;
}

uint8_t* StreamGetBuffer(const MemoryStream* stream)
{
    return stream->buffer;
}
