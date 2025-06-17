#include "CodeGenerator.h"

#include "Writer.h"
#include "passes/BlockExpressionPass.h"
#include "passes/BlockRemoverPass.h"
#include "passes/ControlFlowPass.h"
#include "passes/ForLoopPass.h"
#include "passes/GlobalSectionPass.h"
#include "passes/MemberExpansionPass.h"
#include "passes/Printer.h"
#include "passes/ResolverPass.h"
#include "passes/ReturnTaggingPass.h"
#include "passes/TypeConversionPass.h"
#include "passes/UniqueNamePass.h"
#include "passes/ChainedAssignmentPass.h"
#include "passes/FunctionCallAccessPass.h"
#include "passes/RemoveUnusedPass.h"
#include "passes/CountUsesAndDependenciesPass.h"

Result GenerateCode(const AST* syntaxTree, char** outputCode, size_t* outputLength)
{
	PROPAGATE_ERROR(ResolverPass(syntaxTree));
	ChainedAssignmentPass(syntaxTree);
	FunctionCallAccessPass(syntaxTree);
	BlockExpressionPass(syntaxTree);
	ForLoopPass(syntaxTree);
	ReturnTaggingPass(syntaxTree);
	PROPAGATE_ERROR(MemberExpansionPass(syntaxTree));
	PROPAGATE_ERROR(ControlFlowPass(syntaxTree));
	PROPAGATE_ERROR(TypeConversionPass(syntaxTree));
	GlobalSectionPass(syntaxTree);

	CountUsesAndDependenciesPass(syntaxTree);
	RemoveUnusedPass(syntaxTree);

	UniqueNamePass(syntaxTree);
	BlockRemoverPass(syntaxTree);

	WriteOutput(syntaxTree, outputCode, outputLength);
	return SUCCESS_RESULT;
}
