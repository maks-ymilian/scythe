#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/GlobalSectionPass.h"
#include "passes/ResolverPass.h"
#include "passes/StructExpansionPass.h"
#include "passes/TypeConversionPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	GlobalSectionPass(syntaxTree);
	PROPAGATE_ERROR(StructExpansionPass(syntaxTree));
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
