#include "data-structures/MemoryStream.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define START_SIZE 256

typedef struct MemoryStream
{
    uint8_t* buffer;
    size_t writePos, readPos, capacity;
} MemoryStream;

MemoryStream* AllocateMemoryStream()
{
    MemoryStream* stream = malloc(sizeof(MemoryStream));
    stream->buffer = malloc(START_SIZE * sizeof(uint8_t));
    stream->capacity = START_SIZE;
    stream->writePos = 0;
    stream->readPos = 0;
    return stream;
}

Buffer StreamRead(MemoryStream* stream, const size_t length)
{
    const size_t bytesLeft = stream->writePos - stream->readPos;
    const size_t minLength = length < bytesLeft ? length : bytesLeft;

    assert(minLength != 0);

    const Buffer buffer = (Buffer){&stream->buffer[stream->readPos], minLength};
    stream->readPos += minLength;
    return buffer;
}

Buffer StreamReadRest(MemoryStream* stream)
{
    return StreamRead(stream, stream->writePos - stream->readPos);
}

void StreamRewind(MemoryStream* stream, size_t offset)
{
    if (offset > stream->writePos)
        offset = stream->writePos;

    stream->writePos -= offset;
    if (stream->readPos > stream->writePos)
        stream->readPos = stream->writePos;
}

Buffer FreeMemoryStream(MemoryStream* stream, const bool freeBuffer)
{
    const Buffer buffer = StreamReadRest(stream);
    free(stream);

    if (freeBuffer)
    {
        free(buffer.buffer);
        return (Buffer){NULL, 0};
    }

    return buffer;
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

    if (stream->writePos + length >= stream->capacity)
        Reallocate(stream);

    memcpy(stream->buffer + stream->writePos, buffer, length);
    stream->writePos += length;

    return length;
}

void StreamWriteByte(MemoryStream* stream, const uint8_t data)
{
    StreamWrite(stream, &data, 1);
}
