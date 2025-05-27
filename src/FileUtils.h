#pragma once

#include <stdbool.h>
#include <stddef.h>

int IsSameFile(const char* path1, const char* path2);
char* AllocAbsolutePath(const char* path);
char* AllocFileName(const char* path);
char* AllocFileNameNoExtension(const char* path);
char* AllocDirectoryName(const char* path);
bool ChangeDirectory(const char* path);
bool CheckFileAccess(const char* path, bool read, bool write);
int IsRegularFile(const char* path);
