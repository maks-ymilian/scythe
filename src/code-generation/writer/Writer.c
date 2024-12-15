#include "Writer.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "data-structures/MemoryStream.h"
#include "Result.h"
#include "SyntaxTree.h"

static MemoryStream* stream;

static int CountCharsInNumber(long number)
{
    if (number == 0)
        return 1;

    int minus = 0;
    if (number < 0)
    {
        number = -number;
        minus = 1;
    }

    if (number < 10) return 1 + minus;
    if (number < 100) return 2 + minus;
    if (number < 1000) return 3 + minus;
    if (number < 10000) return 4 + minus;
    if (number < 100000) return 5 + minus;
    if (number < 1000000) return 6 + minus;
    if (number < 10000000) return 7 + minus;
    if (number < 100000000) return 8 + minus;
    if (number < 1000000000) return 9 + minus;
    return 10 + minus;
}

static void WriteInteger(const long integer)
{
    char string[CountCharsInNumber(integer) + 1];
    snprintf(string, sizeof(string), "%ld", integer);
    StreamWrite(stream, string, sizeof(string) - 1);
}

void WriteString(const char* str) { StreamWrite(stream, str, strlen(str)); }
void WriteChar(const char chr) { StreamWriteByte(stream, chr); }

static Result VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
    return SUCCESS_RESULT;
}

static Result VisitVariableDeclaration(const VarDeclStmt* varDecl)
{
    return SUCCESS_RESULT;
}

static Result VisitBlock(const BlockStmt* block)
{
    return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
    const char* sectionText = NULL;
    switch (section->sectionType)
    {
    case Section_Init: sectionText = "init";
        break;
    case Section_Block: sectionText = "block";
        break;
    case Section_Sample: sectionText = "sample";
        break;
    case Section_Serialize: sectionText = "serialize";
        break;
    case Section_Slider: sectionText = "slider";
        break;
    case Section_GFX: sectionText = "gfx";
        break;
    default: assert(0);
    }

    WriteChar('@');
    WriteString(sectionText);
    WriteChar('\n');

    assert(section->block.type == Node_Block);
    HANDLE_ERROR(VisitBlock(section->block.ptr));

    return SUCCESS_RESULT;
}

Result Write(const AST* ast, char** outBuffer, size_t* outLength)
{
    stream = AllocateMemoryStream();

    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* stmt = ast->nodes.array[i];
        switch (stmt->type)
        {
        case Node_Section: return VisitSection(stmt->ptr);
        case Node_Import: return SUCCESS_RESULT;
        default: assert(0);
        }
    }

    const Buffer buffer = StreamGetBuffer(stream);
    FreeMemoryStream(stream, false);
    if (outBuffer != NULL) *outBuffer = (char*)buffer.buffer;
    if (outLength != NULL) *outLength = buffer.length;

    return SUCCESS_RESULT;
}
