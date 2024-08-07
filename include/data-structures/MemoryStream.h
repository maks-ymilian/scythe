#include "inttypes.h"

typedef struct MemoryStream MemoryStream;

MemoryStream* AllocateMemoryStream();
void FreeMemoryStream(MemoryStream* stream);

void StreamWrite(MemoryStream* stream, const void* buffer, size_t length);
uint8_t* StreamGetBuffer(const MemoryStream* stream);