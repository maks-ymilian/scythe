#include "Compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "code-generation/CodeGenerator.h"

typedef struct
{
    AST ast;
    Array dependencies;
}ProgramNode;

static void FreeProgramNode(const ProgramNode* programNode)
{
    FreeSyntaxTree(programNode->ast);
    for (int i = 0; i < programNode->dependencies.length; ++i)
        FreeProgramNode(programNode->dependencies.array[i]);
    FreeArray(&programNode->dependencies);
}

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

static ProgramNode GenerateProgramNode(const char* path)
{
    ProgramNode programNode;

    char* file = GetFileString(path);
    Array tokens;
    HandleError(Scan(file, &tokens), "Scan");
    free(file);

    HandleError(Parse(&tokens, &programNode.ast), "Parse");
    FreeTokenArray(&tokens);

    Array dependencies = AllocateArray(sizeof(ProgramNode));
    for (int i = 0; i < programNode.ast.statements.length; ++i)
    {
        const NodePtr* node = programNode.ast.statements.array[i];
        if (node->type != ImportStatement) break;
        const ImportStmt* importStmt = node->ptr;

        ProgramNode importedNode = GenerateProgramNode(importStmt->file);
        ArrayAdd(&dependencies, &importedNode);
    }
    programNode.dependencies = dependencies;

    return programNode;
}

char* Compile(const char* path, size_t* outLength)
{
    const ProgramNode programTree = GenerateProgramNode(path);

    char* outputCode = NULL;
    size_t outputCodeLength = 0;
    HandleError(GenerateCode(&programTree.ast, (uint8_t**)&outputCode, &outputCodeLength), "Code generation");
    assert(outputCode != NULL);

    FreeProgramNode(&programTree);

    *outLength = outputCodeLength;
    return outputCode;
}