#include "CodeGenerator.h"

#include "StatementGenerator.h"
#include "Common.h"

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* outputLength)
{
    InitResources();

    SetCurrentStream(FunctionsStream);
    WRITE_LITERAL("@init\n");
    SetPreviousStream();

    const Result result = GenerateProgram(syntaxTree);

    const Buffer outputBuffer = CombineStreams();

    FreeSyntaxTree((NodePtr){syntaxTree, RootNode});
    FreeResources();

    if (result.hasError)
        return result;

    *outputCode = outputBuffer.buffer;
    *outputLength = outputBuffer.length;

    return SUCCESS_RESULT;
}
