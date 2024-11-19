#include "CodeGenerator.h"

#include "StatementGenerator.h"
#include "Common.h"

Result GenerateCode(const AST* syntaxTree, uint8_t** outputCode, size_t* outputLength)
{
    InitResources();

    const Result result = GenerateProgram(syntaxTree);

    const Buffer outputBuffer = ReadAll();

    FreeResources();

    if (result.hasError)
    {
        free(outputBuffer.buffer);
        return result;
    }

    *outputCode = outputBuffer.buffer;
    if (outputLength != NULL)
        *outputLength = outputBuffer.length;

    return SUCCESS_RESULT;
}
