#include "Compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "code-generation/CodeGenerator.h"
#include "StringUtils.h"
#include "FileUtils.h"

static Array programNodes;

typedef struct
{
    AST ast;
    Array dependencies;
} ProgramNode;

typedef struct
{
    char* path;
    ProgramNode* programNode;
} ProgramNodeEntry;

static char* GetFileString(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
        return NULL;

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

static void FreeProgramTree()
{
    for (int i = 0; i < programNodes.length; ++i)
    {
        const ProgramNodeEntry* node = programNodes.array[i];
        free(node->path);

        FreeSyntaxTree(node->programNode->ast);
        FreeArray(&node->programNode->dependencies);
        free(node->programNode);
    }
    FreeArray(&programNodes);
}

static void HandleError(const Result result, const char* errorStage, const char* filePath)
{
    if (result.success)
    {
        assert(!result.hasError);
        return;
    }

    assert(result.hasError);
    printf("%s error: ", errorStage);
    PrintError(result);
    if (filePath) printf(" (%s)", filePath);
    printf("\n");
    printf("Press ENTER to continue...\n");
    getchar();
    exit(1);
}

static Result WriteOutputFile(const char* path, const char* code, const size_t length)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL)
    {
        fclose(file);
        return ERROR_RESULT(AllocateString2Str("Failed to open output file \"%s\": %s", path, strerror(errno)), -1);
    }

    if (length == 0)
    {
        fclose(file);
        return SUCCESS_RESULT;
    }

    fwrite(code, sizeof(uint8_t), length, file);
    if (ferror(file))
        return ERROR_RESULT(AllocateString1Str("Failed to write to output file \"%s\"", path), -1);

    fclose(file);
    return SUCCESS_RESULT;
}

static Result IsFileOpenable(const char* path, const int lineNumber)
{
    FILE* file = fopen(path, "rb");

    if (file == NULL)
        return ERROR_RESULT(
            AllocateString2Str("Failed to open input file \"%s\": %s", path, strerror(errno)),
            lineNumber);

    fclose(file);

    return SUCCESS_RESULT;
}

static Result GetSourceFromImportPath(const char* path, const int lineNumber, char** outString)
{
    *outString = GetFileString(path);
    if (*outString == NULL)
        return ERROR_RESULT(
            AllocateString2Str("Failed to open input file \"%s\": %s", path, strerror(errno)),
            lineNumber);

    return SUCCESS_RESULT;
}

static Result CheckForCircularDependency(const ProgramNode* node, const ProgramNode* importedNode, const int lineNumber)
{
    for (int i = 0; i < importedNode->dependencies.length; ++i)
    {
        const ProgramNode* dependency = *(ProgramNode**)importedNode->dependencies.array[i];

        if (dependency == node)
            return ERROR_RESULT("Circular dependency detected", lineNumber);

        const Result result = CheckForCircularDependency(node, dependency, lineNumber);
        if (result.hasError) return result;
    }

    return SUCCESS_RESULT;
}

static ProgramNode* GenerateProgramNode(const char* path, const ImportStmt* importStmt, const char* containingPath)
{
    const int lineNumber = importStmt != NULL ? importStmt->import.lineNumber : -1;
    HandleError(IsFileOpenable(path, lineNumber),
                "Import", containingPath);

    for (int i = 0; i < programNodes.length; ++i)
    {
        const ProgramNodeEntry* currentRecord = programNodes.array[i];

        const int isSameFile = IsSameFile(path, currentRecord->path);
        if (isSameFile == -1)
            HandleError(ERROR_RESULT("Could not compare file paths", lineNumber),
                        "Import", containingPath);

        if (isSameFile)
            return currentRecord->programNode;
    }

    ProgramNode* programNode = malloc(sizeof(ProgramNode));

    const ProgramNodeEntry record = (ProgramNodeEntry){.programNode = programNode, .path = AllocateString(path)};
    ArrayAdd(&programNodes, &record);

    char* source = NULL;
    HandleError(GetSourceFromImportPath(path, lineNumber, &source),
                "Read", containingPath);

    Array tokens;
    HandleError(Scan(source, &tokens),
                "Scan", path);
    free(source);

    HandleError(Parse(&tokens, &programNode->ast),
                "Parse", path);
    FreeTokenArray(&tokens);

    programNode->dependencies = AllocateArray(sizeof(ProgramNode*));
    for (int i = 0; i < programNode->ast.statements.length; ++i)
    {
        const NodePtr* node = programNode->ast.statements.array[i];
        if (node->type != ImportStatement) break;
        const ImportStmt* importStmt = node->ptr;

        ProgramNode* importedNode = GenerateProgramNode(importStmt->file, importStmt, path);
        ArrayAdd(&programNode->dependencies, &importedNode);

        HandleError(CheckForCircularDependency(programNode, importedNode, importStmt->import.lineNumber),
                    "Import", path);
    }

    return programNode;
}

void Compile(const char* inputPath, const char* outputPath)
{
    programNodes = AllocateArray(sizeof(ProgramNodeEntry));
    const ProgramNode* programTree = GenerateProgramNode(inputPath, NULL, NULL);

    char* outputCode = NULL;
    size_t outputCodeLength = 0;
    HandleError(GenerateCode(&programTree->ast, (uint8_t**)&outputCode, &outputCodeLength),
                "Code generation", NULL);

    FreeProgramTree();

    printf("Output code:\n%.*s", (int)outputCodeLength, outputCode);
    HandleError(WriteOutputFile(outputPath, outputCode, outputCodeLength),
                "Write", NULL);

    free(outputCode);
}
