#include "CodeGenerator.h"

#include "analyzer/Analyzer.h"
#include "writer/Writer.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
    HANDLE_ERROR(Analyze(syntaxTree));
    HANDLE_ERROR(Write(syntaxTree, outputCode, outputLength));
    return SUCCESS_RESULT;
}
