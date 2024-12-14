#include "CodeGenerator.h"

#include "analyzer/Analyzer.h"
#include "Common.h"

Result GenerateCode(const AST* syntaxTree, Map* modules, Module* outModule, uint8_t** outputCode, size_t* outputLength)
{
    InitResources(modules);

    HANDLE_ERROR(Analyze(syntaxTree));

    const Buffer outputBuffer = AllocateStreamBuffer();
    FreeResources();

    *outModule = module;

    *outputCode = outputBuffer.buffer;
    if (outputLength != NULL)
        *outputLength = outputBuffer.length;

    return SUCCESS_RESULT;
}
