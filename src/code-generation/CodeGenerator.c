#include "CodeGenerator.h"

#include "passes/GlobalSectionPass.h"
#include "passes/ResolverPass.h"
#include "passes/TypeConversionPass.h"
#include "Writer.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	GlobalSectionPass(syntaxTree);
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));
	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
