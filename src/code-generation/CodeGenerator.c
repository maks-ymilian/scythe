#include "CodeGenerator.h"

#include "StatementGenerator.h"
#include "Common.h"

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length)
{
    InitResources();

    SetCurrentStream(FunctionsStream);
    WRITE_LITERAL("@init\n");
    SetPreviousStream();

    const Result result = GenerateProgram(syntaxTree);

    WRITE_LITERAL("\n");
    const Buffer outputBuffer = CombineStreams();

    FreeSyntaxTree((NodePtr){syntaxTree, RootNode});
    FreeResources();

    if (result.hasError)
        return result;

    *outputCode = outputBuffer.buffer;
    *length = outputBuffer.length;

    return SUCCESS_RESULT;
}
