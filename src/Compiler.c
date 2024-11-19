#include "Compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "code-generation/CodeGenerator.h"

static char* GetFileString(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("Failed to open input file %s: %s\n", path, strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    assert(fileSize != -1L);
    rewind(file);

    char* string = malloc(fileSize + 1);

    fread(string, sizeof(char), fileSize, file);
    fclose(file);
    string[fileSize] = '\0';
    return string;
}

static void HandleError(const Result result, const char* errorStage)
{
    if (result.success)
    {
        assert(!result.hasError);
        return;
    }

    assert(result.hasError);
    printf("%s error: ", errorStage);
    PrintError(result);
    printf("Press ENTER to continue...\n");
    getchar();
    exit(1);
}

char* Compile(const char* path, size_t* outLength)
{
    char* file = GetFileString(path);
    Array tokens;
    HandleError(Scan(file, &tokens), "Scan");
    free(file);

    Program syntaxTree;
    HandleError(Parse(&tokens, &syntaxTree), "Parse");
    FreeTokenArray(&tokens);

    char* outputCode = NULL;
    size_t outputCodeLength = 0;
    HandleError(GenerateCode(&syntaxTree, (uint8_t**)&outputCode, &outputCodeLength), "Code generation");
    assert(outputCode != NULL);
    FreeSyntaxTree(syntaxTree);

    *outLength = outputCodeLength;
    return outputCode;
}