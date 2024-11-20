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

static Array openedFiles;

typedef struct
{
    AST ast;
    Array dependencies;
} ProgramNode;

typedef struct
{
    char* path;
    ProgramNode* programNode;
} FileRecord;

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

static void FreeOpenedFilesArray()
{
    for (int i = 0; i < openedFiles.length; ++i)
    {
        const FileRecord* file = openedFiles.array[i];
        free(file->path);
    }
    FreeArray(&openedFiles);
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

static void FreeProgramNode(ProgramNode* programNode)
{
    FreeSyntaxTree(programNode->ast);
    FreeArray(&programNode->dependencies);
    free(programNode);
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

static ProgramNode* GenerateProgramNode(const char* path, const ImportStmt* importStmt, const char* containingPath)
{
    const int lineNumber = importStmt != NULL ? importStmt->import.lineNumber : -1;
    HandleError(IsFileOpenable(path, lineNumber),
                "Import", containingPath);

    for (int i = 0; i < openedFiles.length; ++i)
    {
        const FileRecord* currentRecord = openedFiles.array[i];

        const int isSameFile = IsSameFile(path, currentRecord->path);
        if (isSameFile == -1)
            HandleError(ERROR_RESULT("Could not compare file paths", lineNumber),
                        "Import", containingPath);

        if (isSameFile)
            return currentRecord->programNode;
    }

    ProgramNode* programNode = malloc(sizeof(ProgramNode));

    const FileRecord record = (FileRecord){.programNode = programNode, .path = AllocateString(path)};
    ArrayAdd(&openedFiles, &record);

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

    Array dependencies = AllocateArray(sizeof(ProgramNode*));
    for (int i = 0; i < programNode->ast.statements.length; ++i)
    {
        const NodePtr* node = programNode->ast.statements.array[i];
        if (node->type != ImportStatement) break;
        const ImportStmt* importStmt = node->ptr;

        ProgramNode* importedNode = GenerateProgramNode(importStmt->file, importStmt, path);
        ArrayAdd(&dependencies, &importedNode);
    }
    programNode->dependencies = dependencies;

    return programNode;
}

char* Compile(const char* path, size_t* outLength)
{
    openedFiles = AllocateArray(sizeof(FileRecord));

    const ProgramNode* programTree = GenerateProgramNode(path, NULL, NULL);

    char* outputCode = NULL;
    size_t outputCodeLength = 0;
    HandleError(GenerateCode(&programTree->ast, (uint8_t**)&outputCode, &outputCodeLength),
                "Code generation", NULL);
    assert(outputCode != NULL);

    for (int i = 0; i < openedFiles.length; ++i)
        FreeProgramNode(((FileRecord*)openedFiles.array[i])->programNode);
    FreeOpenedFilesArray();

    *outLength = outputCodeLength;
    return outputCode;
}
