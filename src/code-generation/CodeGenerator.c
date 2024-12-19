#include "CodeGenerator.h"

#include "passes/GlobalSectionPass.h"
#include "passes/ResolverPass.h"
#include "passes/WriterPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
    PROPAGATE_ERROR(ResolverPass(syntaxTree));
    PROPAGATE_ERROR(GlobalSectionPass(syntaxTree));
    PROPAGATE_ERROR(WriterPass(syntaxTree, outputCode, outputLength));
    return SUCCESS_RESULT;
}
