#include "Writer.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "data-structures/MemoryStream.h"
#include "Result.h"
#include "SyntaxTree.h"

static MemoryStream* stream;

static void WriteInteger(const long integer)
{
    const int size = snprintf(NULL, 0, "%ld", integer);
    assert(size > 0);
    char string[size + 1];
    snprintf(string, sizeof(string), "%ld", integer);
    StreamWrite(stream, string, sizeof(string) - 1);
}

static void WriteFloat(const double value)
{
    const int size = snprintf(NULL, 0,"%.14lf\n", value);
    assert(size > 0);
    char string[size + 1];
    snprintf(NULL, 0,"%.14lf\n", value);
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

static Result VisitModule(const ModuleNode* module)
{
    if (module->statements.length != 0)
    {
        WriteString("\n// Module: ");
        WriteString(module->path);
        WriteChar('\n');
    }

    for (int i = 0; i < module->statements.length; ++i)
    {
        const NodePtr* stmt = module->statements.array[i];
        switch (stmt->type)
        {
        case Node_Section: return VisitSection(stmt->ptr);
        case Node_Import: return SUCCESS_RESULT;
        default: assert(0);
        }
    }

    return SUCCESS_RESULT;
}

Result Write(const AST* ast, char** outBuffer, size_t* outLength)
{
    stream = AllocateMemoryStream();

    for (int i = 0; i < ast->nodes.length; ++i)
    {
        const NodePtr* node = ast->nodes.array[i];
        assert(node->type == Node_Module);
        HANDLE_ERROR(VisitModule(node->ptr));
    }

    const Buffer buffer = StreamGetBuffer(stream);
    FreeMemoryStream(stream, false);
    if (outBuffer != NULL) *outBuffer = (char*)buffer.buffer;
    if (outLength != NULL) *outLength = buffer.length;

    return SUCCESS_RESULT;
}
