#include "FileUtils.h"

#include "StringUtils.h"

#if defined(_WIN32)

#include <windows.h>
#include <fileapi.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#define BUFSIZE 4096

int IsSameFile(const char* path1, const char* path2)
{
	HANDLE file1 = INVALID_HANDLE_VALUE;
	HANDLE file2 = INVALID_HANDLE_VALUE;

	file1 = CreateFileA(
		path1,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (file1 == INVALID_HANDLE_VALUE) goto error;

	file2 = CreateFileA(
		path2,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (file2 == INVALID_HANDLE_VALUE) goto error;

	BY_HANDLE_FILE_INFORMATION file1Info;
	if (!GetFileInformationByHandle(file1, &file1Info)) goto error;

	BY_HANDLE_FILE_INFORMATION file2Info;
	if (!GetFileInformationByHandle(file2, &file2Info)) goto error;

	CloseHandle(file1);
	CloseHandle(file2);

	return file1Info.dwVolumeSerialNumber == file2Info.dwVolumeSerialNumber &&
		   file1Info.nFileIndexLow == file2Info.nFileIndexLow &&
		   file1Info.nFileIndexHigh == file2Info.nFileIndexHigh;

error:
	if (file1 != INVALID_HANDLE_VALUE) CloseHandle(file1);
	if (file2 != INVALID_HANDLE_VALUE) CloseHandle(file2);

	return -1;
}

char* AllocAbsolutePath(const char* path)
{
	char fullName[BUFSIZE];
	DWORD result = GetFullPathNameA(path, sizeof(fullName), fullName, NULL);
	if (result == 0 || result == sizeof(fullName))
		goto error;

	DWORD numBytes = result + 1;
	char* out = malloc(numBytes);
	if (!out) goto error;
	memcpy(out, fullName, numBytes);
	out[numBytes - 1] = '\0';
	return out;

error:
	return NULL;
}

char* AllocFileName(const char* path)
{
	if (!path)
		goto error;

	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	if (_splitpath_s(path, NULL, 0, NULL, 0, fname, sizeof(fname), ext, sizeof(ext)) != 0)
		goto error;

	char combined[_MAX_FNAME + _MAX_EXT];
	if (strcpy_s(combined, sizeof(combined), fname) != 0) goto error;
	if (strcat_s(combined, sizeof(combined), ext) != 0) goto error;

	size_t length = strnlen_s(combined, sizeof(combined));
	if (length == sizeof(combined)) goto error;

	char* out = malloc(length + 1);
	if (!out) goto error;
	memcpy(out, combined, length + 1);
	return out;

error:
	return NULL;
}

char* AllocFileNameNoExtension(const char* path)
{
	if (!path)
		goto error;

	char fname[_MAX_FNAME];
	if (_splitpath_s(path, NULL, 0, NULL, 0, fname, sizeof(fname), NULL, 0) != 0)
		goto error;

	size_t length = strnlen_s(fname, sizeof(fname));
	if (length == sizeof(fname)) goto error;

	char* out = malloc(length + 1);
	if (!out) goto error;
	memcpy(out, fname, length + 1);
	return out;

error:
	return NULL;
}

char* AllocDirectoryName(const char* path)
{
	if (!path)
		goto error;

	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	if (_splitpath_s(path, drive, sizeof(drive), dir, sizeof(dir), NULL, 0, NULL, 0) != 0)
		goto error;

	char combined[_MAX_DRIVE + _MAX_DIR];
	if (strcpy_s(combined, sizeof(combined), drive) != 0) goto error;
	if (strcat_s(combined, sizeof(combined), dir) != 0) goto error;

	size_t length = strnlen_s(combined, sizeof(combined));
	if (length == sizeof(combined)) goto error;

	char* out = malloc(length + 1);
	if (!out) goto error;
	memcpy(out, combined, length + 1);
	return out;

error:
	return NULL;
}

bool ChangeDirectory(const char* path)
{
	return SetCurrentDirectory(path);
}

bool CheckFileAccess(const char* path, bool read, bool write)
{
	return _access(path, (read ? 4 : 0) | (write ? 2 : 0)) == 0;
}

#elif defined(__linux__)

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int IsSameFile(const char* path1, const char* path2)
{
	struct stat stat1;
	if (stat(path1, &stat1) == -1)
		goto error;

	struct stat stat2;
	if (stat(path2, &stat2) == -1)
		goto error;

	return stat1.st_dev == stat2.st_dev &&
		   stat1.st_ino == stat2.st_ino;

error:
	return -1;
}

char* AllocAbsolutePath(const char* path)
{
	return realpath(path, NULL);
}

char* AllocFileName(const char* path)
{
	if (!path)
		goto error;

	char* copy = AllocateString(path);
	char* baseName = basename(copy);
	char* out = AllocateString(baseName);
	free(copy);
	return out;

error:
	return NULL;
}

char* AllocFileNameNoExtension(const char* path)
{
	if (!path)
		goto error;

	char* copy = AllocateString(path);
	char* baseName = basename(copy);

	size_t baseNameLength = strlen(baseName);
	size_t numChars = baseNameLength;
	for (size_t i = 0; i < baseNameLength; ++i)
	{
		if (baseName[i] == '.')
		{
			numChars = i;
			break;
		}
	}
	if (numChars == 0)
		goto error;

	char* out = malloc(numChars + 1);
	if (!out) goto error;
	memcpy(out, baseName, numChars);
	out[numChars] = '\0';
	return out;

error:
	return NULL;
}

char* AllocDirectoryName(const char* path)
{
	if (!path)
		goto error;

	char* copy = AllocateString(path);
	char* baseName = dirname(copy);
	char* out = AllocateString(baseName);
	free(copy);
	return out;

error:
	return NULL;
}

bool ChangeDirectory(const char* path)
{
	return chdir(path) == 0;
}

bool CheckFileAccess(const char* path, bool read, bool write)
{
	int permissions = (read ? R_OK : 0) | (write ? W_OK : 0);
	return access(path, !read && !write ? F_OK : permissions) == 0;
}

int IsRegularFile(const char* path)
{
	struct stat status;
	if (stat(path, &status) != 0)
		goto error;

	return S_ISREG(status.st_mode);

error:
	return -1;
}

#endif
