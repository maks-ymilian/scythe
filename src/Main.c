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
	exit(1);\
}else assert(result.success);\
}

static void Compile(const char* source)
{
    Array tokens;
    HANDLE_ERROR(Scan(source, &tokens), "Scan error");

    StmtPtr syntaxTree;
    HANDLE_ERROR(Parse(&tokens, &syntaxTree), "Parse error");

    HANDLE_ERROR(GenerateCode(syntaxTree.ptr), "Code generation error");
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
