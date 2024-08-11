#include "data-structures/MemoryStream.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define START_SIZE 256

typedef struct MemoryStream
{
    uint8_t* buffer;
    size_t position, length, capacity;
} MemoryStream;

MemoryStream* AllocateMemoryStream()
{
    MemoryStream* stream = malloc(sizeof(MemoryStream));
    stream->buffer = malloc(START_SIZE * sizeof(uint8_t));
    stream->capacity = START_SIZE;
    stream->position = 0;
    stream->length = 0;
    return stream;
}

Buffer StreamRead(MemoryStream* stream, const size_t length)
{
    const size_t bytesLeft = stream->position - stream->length;
    const size_t minLength = length < bytesLeft ? length : bytesLeft;

    assert(minLength != 0);

    const Buffer buffer = (Buffer){&stream->buffer[stream->position], minLength};
    stream->position += minLength;
    return buffer;
}

void StreamRewind(MemoryStream* stream, size_t offset)
{
    if (offset == 0)
        stream->position = 0;

    if (offset > stream->position)
        offset = stream->position;

    stream->position -= offset;
}

Buffer StreamRewindRead(MemoryStream* stream, const size_t offset)
{
    const size_t position = stream->position;
    StreamRewind(stream, offset);
    return StreamRead(stream, position - stream->position);
}

size_t StreamGetPosition(const MemoryStream* stream) { return stream->position; }

void FreeMemoryStream(MemoryStream* stream, const bool freeBuffer)
{
    if (freeBuffer)
        free(stream->buffer);
    free(stream);
}

static void Reallocate(MemoryStream* stream)
{
    stream->capacity *= 2;
    stream->buffer = realloc(stream->buffer, stream->capacity);
}

size_t StreamWrite(MemoryStream* stream, const void* buffer, const size_t length)
{
    assert(buffer != NULL);
    assert(length > 0);

    if (stream->position + length >= stream->capacity)
        Reallocate(stream);

    memmove(stream->buffer + stream->position, buffer, length);
    stream->position += length;
    if (stream->position > stream->length)
        stream->length = stream->position;

    return length;
}

void StreamWriteByte(MemoryStream* stream, const uint8_t data)
{
    StreamWrite(stream, &data, 1);
}
