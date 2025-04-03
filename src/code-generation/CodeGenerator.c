#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/BlockExpressionPass.h"
#include "passes/BlockRemoverPass.h"
#include "passes/CallArgumentPass.h"
#include "passes/ControlFlowPass.h"
#include "passes/ForLoopPass.h"
#include "passes/GlobalSectionPass.h"
#include "passes/MemberExpansionPass.h"
#include "passes/Printer.h"
#include "passes/ResolverPass.h"
#include "passes/ReturnTaggingPass.h"
#include "passes/TypeConversionPass.h"
#include "passes/UniqueNamePass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	BlockExpressionPass(syntaxTree);
	ForLoopPass(syntaxTree);
	ReturnTaggingPass(syntaxTree);
	PROPAGATE_ERROR(CallArgumentPass(syntaxTree));
	PROPAGATE_ERROR(MemberExpansionPass(syntaxTree));
	PROPAGATE_ERROR(ControlFlowPass(syntaxTree));
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));
	UniqueNamePass(syntaxTree);
	GlobalSectionPass(syntaxTree);
	BlockRemoverPass(syntaxTree);

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
