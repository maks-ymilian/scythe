#include "FileUtils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#ifdef _WIN32
int IsSameFile(const char* path1, const char* path2)
{
    const HANDLE file1 = CreateFileA(
        path1,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (file1 == INVALID_HANDLE_VALUE) return -1;

    const HANDLE file2 = CreateFileA(
        path2,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (file2 == INVALID_HANDLE_VALUE) return -1;

    BY_HANDLE_FILE_INFORMATION file1Info;
    if (!GetFileInformationByHandle(file1, &file1Info)) return -1;

    BY_HANDLE_FILE_INFORMATION file2Info;
    if (!GetFileInformationByHandle(file2, &file2Info)) return -1;

    if (!CloseHandle(file1)) return -1;
    if (!CloseHandle(file2)) return -1;

    return
            file1Info.dwVolumeSerialNumber == file2Info.dwVolumeSerialNumber &&
            file1Info.nFileIndexLow == file2Info.nFileIndexLow &&
            file1Info.nFileIndexHigh == file2Info.nFileIndexHigh;
}
#else
int IsSameFile(const char* path1, const char* path2)
{
    struct stat stat1;
    if (stat(path1, &stat1) == -1)
        return -1;

    struct stat stat2;
    if (stat(path2, &stat2) == -1)
        return -1;

    return
            stat1.st_dev == stat2.st_dev &&
            stat1.st_ino == stat2.st_ino;
}
#endif
