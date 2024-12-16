#include "CodeGenerator.h"

#include "passes/GlobalSectionPass.h"
#include "passes/ResolverPass.h"
#include "passes/WriterPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
    HANDLE_ERROR(ResolverPass(syntaxTree));
    HANDLE_ERROR(GlobalSectionPass(syntaxTree));
    HANDLE_ERROR(WriterPass(syntaxTree, outputCode, outputLength));
    return SUCCESS_RESULT;
}
