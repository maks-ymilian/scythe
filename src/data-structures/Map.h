#pragma once

#include <inttypes.h>
#include <stddef.h>

#define MAP_ITERATE(name, map) \
	Node* name = NULL;         \
	MapNext(map, &name);

typedef struct Node Node;

struct Node
{
	char* key;
	void* value;
	Node* next;
	size_t bucket;
};

typedef struct
{
	Node** buckets;
	size_t elementCount, bucketCount, bucketsCap;
	size_t sizeOfValueType;
} Map;

Map AllocateMap(size_t sizeOfValueType);
void FreeMap(const Map* map);
void* MapGet(const Map* map, const char* key);
bool MapNext(const Map* map, Node** current);
bool MapAdd(Map* map, const char* key, const void* value);
bool MapRemove(Map* map, const char* key);
