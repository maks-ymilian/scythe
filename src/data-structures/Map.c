#include "data-structures/Map.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define LOAD_FACTOR 0.75
#define START_SIZE 16

static Map AllocateMapSize(const size_t sizeOfValueType, const size_t count)
{
	Map map;
	map.bucketsSize = count;
	map.elementCount = 0;
	map.sizeOfValueType = sizeOfValueType;
	map.buckets = calloc(count, sizeof(Node*));

	return map;
}

Map AllocateMap(const size_t sizeOfValueType)
{
	return AllocateMapSize(sizeOfValueType, START_SIZE);
}

void FreeMap(const Map* map)
{
	for (size_t i = 0; i < map->bucketsSize; ++i)
	{
		Node* node = map->buckets[i];
		while (true)
		{
			if (node == NULL)
				break;

			Node* next = node->next;
			free(node->key);
			free(node->value);
			free(node);

			node = next;
		}
	}

	free(map->buckets);
}

static size_t Hash(const char* string)
{
	size_t hash = 5381;
	size_t c;

	while ((c = (size_t)*string++))
		hash = (hash << 5) + hash + c;

	return hash;
}

static size_t GetIndex(const Map* map, const char* string)
{
	const size_t hash = Hash(string);
	return hash % map->bucketsSize;
}

static void AddNode(Map* map, Node* node)
{
	node->next = NULL;

	const size_t index = GetIndex(map, node->key);
	node->bucket = index;
	Node* get = map->buckets[index];
	if (get == NULL)
	{
		map->buckets[index] = node;
	}
	else
	{
		map->buckets[index] = node;
		node->next = get;
	}

	map->elementCount++;
}

static void Expand(Map* map)
{
	Map newMap = AllocateMapSize(map->sizeOfValueType, map->bucketsSize * 2);

	for (size_t i = 0; i < map->bucketsSize; ++i)
	{
		Node* node = map->buckets[i];
		while (true)
		{
			if (node == NULL)
				break;

			Node* next = node->next;
			AddNode(&newMap, node);

			node = next;
		}
	}

	free(map->buckets);

	*map = newMap;
}

void* MapGet(const Map* map, const char* key)
{
	const Node* node = map->buckets[GetIndex(map, key)];

	while (true)
	{
		if (node == NULL)
			return NULL;

		if (!strcmp(key, node->key))
			return node->value;

		node = node->next;
	}
}

static Node* NextBucket(const Map* map, const size_t startIndex)
{
	for (size_t i = startIndex; i < map->bucketsSize; ++i)
	{
		Node* node = map->buckets[i];
		if (node != NULL)
			return node;
	}
	return NULL;
}

bool MapNext(const Map* map, Node** current)
{
	assert(current != NULL);

	if (*current == NULL)
	{
		*current = NextBucket(map, 0);
		return *current != NULL;
	}

	const Node* currentNode = *current;
	Node* next = currentNode->next;
	if (next == NULL)
	{
		next = NextBucket(map, currentNode->bucket + 1);
		if (next == NULL) return false;
	}

	*current = next;

	return true;
}

static Node* AllocateNode(const Map* map, const char* key, const void* value)
{
	Node* node = malloc(sizeof(Node));

	const size_t keyLength = strlen(key);
	node->key = malloc(keyLength + 1);
	memcpy(node->key, key, keyLength + 1);

	node->value = malloc(map->sizeOfValueType);
	memcpy(node->value, value, map->sizeOfValueType);

	node->next = NULL;

	return node;
}

bool MapAdd(Map* map, const char* key, const void* value)
{
	assert(map != NULL);

	if (MapGet(map, key) != NULL)
		return false;

	double filled = (double)map->elementCount / (double)map->bucketsSize;
	if (filled > LOAD_FACTOR)
		Expand(map);

	Node* node = AllocateNode(map, key, value);
	AddNode(map, node);

	return true;
}

bool MapRemove(Map* map, const char* key)
{
	const size_t index = GetIndex(map, key);
	Node* node = map->buckets[index];
	Node* prevNode = NULL;

	while (true)
	{
		if (node == NULL)
			return false;

		if (!strcmp(key, node->key))
			break;

		prevNode = node;
		node = node->next;
	}

	if (prevNode == NULL)
		map->buckets[index] = node->next;
	else
		prevNode->next = node->next;

	free(node->key);
	free(node->value);
	free(node);

	map->elementCount--;

	return true;
}
