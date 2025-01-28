#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/GlobalSectionPass.h"
#include "passes/MemberExpansionPass.h"
#include "passes/ResolverPass.h"
#include "passes/TypeConversionPass.h"
#include "passes/UniqueNamePass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	GlobalSectionPass(syntaxTree);
	MemberExpansionPass(syntaxTree);
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));
	UniqueNamePass(syntaxTree);

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
