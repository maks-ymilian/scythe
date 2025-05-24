#pragma once

#include <stddef.h>
#include <stdbool.h>

int WindowsCallMain(int (*main)(int argc, char** argv), int argc, wchar_t* argv[]);

int IsSameFile(const char* path1, const char* path2);
char* AllocAbsolutePath(const char* path);
char* AllocFileName(const char* path);
char* AllocFileNameNoExtension(const char* path);
char* AllocDirectoryName(const char* path);
bool ChangeDirectory(const char* path);
