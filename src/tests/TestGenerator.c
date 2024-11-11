#include "TestGenerator.h"

#include <stdlib.h>

#include "data-structures/MemoryStream.h"

#define WRITE_LITERAL(text) StreamWrite(stream, text, sizeof(text) - 1)
#define WRITE_TEXT(text) StreamWrite(stream, text, strlen(text));

static MemoryStream* stream = NULL;

char* GenerateTestSource()
{
    stream = AllocateMemoryStream();

    WRITE_LITERAL("init{");

    WRITE_LITERAL("int a = 5;");

    WRITE_LITERAL("}");

    WRITE_LITERAL("\0");

    const Buffer buffer = StreamGetBuffer(stream);
    FreeMemoryStream(stream, false);
    return (char*)buffer.buffer;
}
