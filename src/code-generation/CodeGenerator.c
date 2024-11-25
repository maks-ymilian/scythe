#include "CodeGenerator.h"

#include "StatementGenerator.h"
#include "Common.h"

static Result GenerateAST(const AST* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(GenerateStatement(node));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(const AST* syntaxTree, Map* modules, Module* outModule, uint8_t** outputCode, size_t* outputLength)
{
    InitResources(modules);

    const Result result = GenerateAST(syntaxTree);

    const Buffer outputBuffer = ReadAll();

    FreeResources();

    if (result.hasError)
    {
        free(outputBuffer.buffer);
        return result;
    }

    *outModule = module;

    *outputCode = outputBuffer.buffer;
    if (outputLength != NULL)
        *outputLength = outputBuffer.length;

    return SUCCESS_RESULT;
}
