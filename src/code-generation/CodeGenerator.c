#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/BlockExpressionPass.h"
#include "passes/ControlFlowPass.h"
#include "passes/GlobalSectionPass.h"
#include "passes/MemberExpansionPass.h"
#include "passes/ResolverPass.h"
#include "passes/TypeConversionPass.h"
#include "passes/UniqueNamePass.h"
#include "passes/ReturnTaggingPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	BlockExpressionPass(syntaxTree);
	ReturnTaggingPass(syntaxTree);
	PROPAGATE_ERROR(MemberExpansionPass(syntaxTree));
	PROPAGATE_ERROR(ControlFlowPass(syntaxTree));
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));
	UniqueNamePass(syntaxTree);
	GlobalSectionPass(syntaxTree);

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
