#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/GlobalSectionPass.h"
#include "passes/ResolverPass.h"
#include "passes/MemberExpansionPass.h"
#include "passes/TypeConversionPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	GlobalSectionPass(syntaxTree);
	PROPAGATE_ERROR(MemberExpansionPass(syntaxTree));
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
