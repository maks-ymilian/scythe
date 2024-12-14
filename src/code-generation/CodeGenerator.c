#include "CodeGenerator.h"

#include "analyzer/Analyzer.h"
#include "writer/Writer.h"
#include "Common.h"

Result GenerateCode(const AST* syntaxTree, Map* modules, Module* outModule, uint8_t** outputCode, size_t* outputLength)
{
    InitResources(modules);

    HANDLE_ERROR(Analyze(syntaxTree));
    HANDLE_ERROR(WriteAST(syntaxTree));

    const Buffer outputBuffer = AllocateStreamBuffer();
    FreeResources();

    *outModule = module;

    *outputCode = outputBuffer.buffer;
    if (outputLength != NULL)
        *outputLength = outputBuffer.length;

    return SUCCESS_RESULT;
}
