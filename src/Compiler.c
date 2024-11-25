#include "Compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "data-structures/MemoryStream.h"
#include "code-generation/CodeGenerator.h"
#include "StringUtils.h"
#include "FileUtils.h"

static Array programNodes;

typedef struct
{
    AST ast;
    Array dependencies;
    Map publicSymbolTable;
    char* path;
    bool searched;
} ProgramNode;

typedef struct
{
    ProgramNode* node;
    int importLineNumber;
} ProgramDependency;

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
        ProgramNode* node = *(ProgramNode**)programNodes.array[i];
        FreeSyntaxTree(node->ast);
        FreeArray(&node->dependencies);
        FreeMap(&node->publicSymbolTable);
        free(node->path);
        free(node);
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
        const ProgramDependency* dependency = importedNode->dependencies.array[i];

        if (dependency->node == node)
            return ERROR_RESULT("Circular dependency detected", lineNumber);

        const Result result = CheckForCircularDependency(node, dependency->node, lineNumber);
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
        ProgramNode* node = *(ProgramNode**)programNodes.array[i];

        const int isSameFile = IsSameFile(path, node->path);
        if (isSameFile == -1)
            HandleError(ERROR_RESULT("Could not compare file paths", lineNumber),
                        "Import", containingPath);

        if (isSameFile)
            return node;
    }

    ProgramNode* programNode = malloc(sizeof(ProgramNode));
    ArrayAdd(&programNodes, &programNode);

    programNode->path = AllocateString(path);

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

    programNode->dependencies = AllocateArray(sizeof(ProgramDependency));
    for (int i = 0; i < programNode->ast.statements.length; ++i)
    {
        const NodePtr* node = programNode->ast.statements.array[i];
        if (node->type != Node_Import) break;
        const ImportStmt* importStmt = node->ptr;

        ProgramDependency dependency =
        {
            .node = GenerateProgramNode(importStmt->file, importStmt, path),
            .importLineNumber = importStmt->import.lineNumber
        };
        ArrayAdd(&programNode->dependencies, &dependency);

        HandleError(CheckForCircularDependency(programNode, dependency.node, importStmt->import.lineNumber),
                    "Import", path);
    }

    programNode->searched = false;
    return programNode;
}

static void SortVisit(ProgramNode* node, Array* array)
{
    if (node->searched == true)
        return;

    for (int i = 0; i < node->dependencies.length; ++i)
        SortVisit(((ProgramDependency*)node->dependencies.array[i])->node, array);

    node->searched = true;
    ArrayAdd(array, &node);
}

static Array SortProgramTree()
{
    Array array = AllocateArray(sizeof(ProgramNode*));
    for (int i = 0; i < programNodes.length; ++i)
    {
        ProgramNode* node = *(ProgramNode**)programNodes.array[i];
        if (node->searched == 0)
            SortVisit(node, &array);
    }
    return array;
}

void CompileProgramTree(char** outCode, size_t* outLength)
{
    MemoryStream* stream = AllocateMemoryStream();

    const Array programNodes = SortProgramTree();
    for (int i = 0; i < programNodes.length; ++i)
    {
        ProgramNode* node = *(ProgramNode**)programNodes.array[i];

        Map publicSymbolTables = AllocateMap(sizeof(Map));

        for (int i = 0; i < node->dependencies.length; ++i)
        {
            const ProgramDependency* dependency = node->dependencies.array[i];

            char path[strlen(dependency->node->path) + 1];
            memcpy(path, dependency->node->path, sizeof(path));
            char* moduleName = basename(path);

            const size_t baseNameLength = strlen(moduleName) + 1;
            for (int i = 0; i < baseNameLength; ++i)
                if (moduleName[i] == '.') moduleName[i] = '\0';

            if (!MapAdd(&publicSymbolTables, moduleName, &dependency->node->publicSymbolTable))
                HandleError(ERROR_RESULT(AllocateString1Str("Module \"%s\" is already defined", moduleName),
                                         dependency->importLineNumber), "Import", node->path);
        }

        char* code = NULL;
        size_t codeLength = 0;
        HandleError(GenerateCode(&node->ast, &node->publicSymbolTable, (uint8_t**)&code, &codeLength),
                    "Code generation", NULL);

        FreeMap(&publicSymbolTables);

        StreamWrite(stream, code, codeLength);
        free(code);
    }
    FreeArray(&programNodes);

    const Buffer buffer = StreamGetBuffer(stream);
    FreeMemoryStream(stream, false);

    *outCode = (char*)buffer.buffer;
    *outLength = buffer.length;
}

void Compile(const char* inputPath, const char* outputPath)
{
    programNodes = AllocateArray(sizeof(ProgramNode*));
    GenerateProgramNode(inputPath, NULL, NULL);

    char* outputCode = NULL;
    size_t outputCodeLength = 0;
    CompileProgramTree(&outputCode, &outputCodeLength);

    FreeProgramTree();

    printf("Output code:\n%.*s", (int)outputCodeLength, outputCode);
    HandleError(WriteOutputFile(outputPath, outputCode, outputCodeLength),
                "Write", NULL);

    free(outputCode);
}
