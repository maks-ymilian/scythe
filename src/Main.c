#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "CodeGenerator.h"

#define HANDLE_ERROR(func, message)\
{\
Result result = func;\
if (result.hasError)\
{\
	printf(message": ");\
	PrintError(result);\
	printf("Press ENTER to continue...\n");\
	getchar();\
	exit(1);\
}else assert(result.success);\
}

static void Compile(const char* source)
{
    Array tokens;
    HANDLE_ERROR(Scan(source, &tokens), "Scan error");

    NodePtr syntaxTree;
    HANDLE_ERROR(Parse(&tokens, &syntaxTree), "Parse error");

    uint8_t* outputCode = NULL;
    size_t outputCodeLength = 0;
    HANDLE_ERROR(GenerateCode(syntaxTree.ptr, &outputCode, &outputCodeLength), "Code generation error");
    assert(outputCode != NULL);

    printf("Output code:\n%.*s", (int)outputCodeLength, (char*)outputCode);

    const char* path = "D:/Program Files/REAPER (x64)/Effects/jjjjjjjjjjjjjjjj/test.jsfx";
    FILE* file = fopen(path, "wb");
    if (file == NULL)
    {
        perror("Failed to open/create file: ");
        exit(1);
    }
    const size_t bytesWritten = fwrite(outputCode, sizeof(uint8_t), outputCodeLength, file);
    if (bytesWritten < outputCodeLength)
    {
        printf("An error occurred when writing to file %s\nBytes written: %llu\nTotal bytes: %llu", path, bytesWritten, outputCodeLength);
        exit(1);
    }

    free(outputCode);
}

int main(void)
{
    FILE* file = fopen("D:/Program Files/REAPER (x64)/Effects/jjjjjjjjjjjjjjjj/test.txt", "rb");

    if (file == NULL)
    {
        perror("Failed to open file: ");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    rewind(file);

    if (fileSize == -1L)
    {
        perror("ftell failed: ");
        return 1;
    }

    char* buffer = malloc(fileSize + 1);

    if (buffer == NULL)
    {
        perror("malloc failed: ");
        return 1;
    }

    fread(buffer, (size_t)fileSize, 1, file);
    fclose(file);
    buffer[fileSize] = 0;

    Compile(buffer);

    free(buffer);

    return 0;
}
