#include "ControlFlowPass.h"

#include "Common.h"
#include "StringUtils.h"

typedef struct
{
	VarDeclStmt* returnFlagDecl;
	VarDeclStmt* returnValueDecl;
} ReturnVariables;

typedef struct
{
	VarDeclStmt* breakFlagDecl;
	VarDeclStmt* continueFlagDecl;
} WhileVariables;

static const char* currentFilePath = NULL;

static const char* breakFlagName = "break";
static const char* continueFlagName = "continue";

static const char* returnFlagName = "return";
static const char* returnValueName = "returnValue";

static NodePtr AllocFlagDecl(const char* name, const int lineNumber)
{
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = lineNumber,
			.name = AllocateString(name),
			.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
			.uniqueName = -1,
			.type.modifier = TypeModifier_None,
			.type.expr = AllocPrimitiveType(Primitive_Bool, lineNumber),
			.initializer = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_Boolean,
					.boolean = false,
				},
				sizeof(LiteralExpr), Node_Literal),
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
}

static NodePtr AllocSetFlag(VarDeclStmt* flagDecl, const int lineNumber)
{
	return AllocSetVariable(
		flagDecl,
		AllocASTNode(
			&(LiteralExpr){
				.lineNumber = lineNumber,
				.type = Literal_Boolean,
				.boolean = true,
			},
			sizeof(LiteralExpr), Node_Literal),
		lineNumber);
}

static NodePtr AllocBreakFlagExpression(VarDeclStmt* breakFlagDecl, const NodePtr inside, const int lineNumber)
{
	return AllocASTNode(
		&(BinaryExpr){
			.lineNumber = lineNumber,
			.operatorType = Binary_BoolAnd,
			.right = inside,
			.left = AllocASTNode(
				&(UnaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Unary_Negate,
					.expression = AllocIdentifier(breakFlagDecl, lineNumber),
				},
				sizeof(UnaryExpr), Node_Unary),
		},
		sizeof(BinaryExpr), Node_Binary);
}

static NodePtr AllocIfFlagIsFalse(VarDeclStmt* flagDecl, Array* statements, const int lineNumber)
{
	return AllocASTNode(
		&(IfStmt){
			.lineNumber = lineNumber,
			.falseStmt = NULL_NODE,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_IsEqual,
					.left = AllocIdentifier(flagDecl, lineNumber),
					.right = AllocASTNode( // clang-format is bad
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Boolean,
							.boolean = false,
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
			.trueStmt = AllocASTNode( // clang-format is bad
				&(BlockStmt){
					.lineNumber = lineNumber,
					.statements = *statements,
				},
				sizeof(BlockStmt), Node_BlockStatement),
		},
		sizeof(IfStmt), Node_If);
}

static NodePtr AllocReturnValueDecl(const Type type, const int lineNumber)
{
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = lineNumber,
			.name = AllocateString(returnValueName),
			.instantiatedVariables = AllocateArray(sizeof(VarDeclStmt*)),
			.uniqueName = -1,
			.type.expr = CopyASTNode(type.expr),
			.type.modifier = type.modifier,
			.initializer = NULL_NODE,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
}

static NodePtr AllocReturnValueStatement(VarDeclStmt* returnValueDecl, const int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = lineNumber,
			.expr = AllocIdentifier(returnValueDecl, lineNumber),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

static void MoveStatements(BlockStmt* block, size_t startIndex, Array* dest)
{
	for (size_t i = startIndex; i < block->statements.length; i++)
		ArrayAdd(dest, block->statements.array[i]);

	for (size_t i = block->statements.length - 1; i >= startIndex; i--)
		ArrayRemove(&block->statements, i);
}

static bool StatementReturns(const NodePtr* node, bool allPaths)
{
	switch (node->type)
	{
	case Node_Return:
	case Node_LoopControl:
		return true;
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			if (StatementReturns(block->statements.array[i], allPaths))
				return true;
		return false;
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		const bool trueReturns = StatementReturns(&ifStmt->trueStmt, allPaths);
		const bool falseReturns = StatementReturns(&ifStmt->falseStmt, allPaths);
		return allPaths ? trueReturns && falseReturns : trueReturns || falseReturns;
	}
	case Node_While:
	{
		const WhileStmt* whileStmt = node->ptr;
		return allPaths ? false : StatementReturns(&whileStmt->stmt, allPaths);
	}
	case Node_FunctionDeclaration:
	case Node_ExpressionStatement:
	case Node_VariableDeclaration:
	case Node_Null:
		return false;
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitLoopControlStatement(NodePtr* node, const WhileVariables* variables)
{
	ASSERT(node->type == Node_LoopControl);
	LoopControlStmt* loopControl = node->ptr;

	if (variables == NULL)
		return ERROR_RESULT(
			loopControl->type == LoopControl_Break
				? "\"break\" is not allowed here"
				: "\"continue\" is not allowed here",
			loopControl->lineNumber,
			currentFilePath);

	*node = AllocASTNode(
		&(BlockStmt){
			.lineNumber = loopControl->lineNumber,
			.statements = AllocateArray(sizeof(NodePtr)),
		},
		sizeof(BlockStmt), Node_BlockStatement);
	BlockStmt* block = node->ptr;

	NodePtr continueFlag = AllocSetFlag(variables->continueFlagDecl, loopControl->lineNumber);
	ArrayAdd(&block->statements, &continueFlag);

	if (loopControl->type == LoopControl_Break)
	{
		NodePtr breakFlag = AllocSetFlag(variables->breakFlagDecl, loopControl->lineNumber);
		ArrayAdd(&block->statements, &breakFlag);
	}

	FreeASTNode((NodePtr){.ptr = loopControl, .type = Node_LoopControl});

	return SUCCESS_RESULT;
}

static Result VisitReturnStatement(
	NodePtr* node,
	const ReturnVariables* returnVars,
	const WhileVariables* whileVars,
	bool isVoid)
{
	ASSERT(node->type == Node_Return);
	ReturnStmt* returnStmt = node->ptr;

	Array statements = AllocateArray(sizeof(NodePtr));

	NodePtr setFlag = AllocSetFlag(returnVars->returnFlagDecl, returnStmt->lineNumber);
	ArrayAdd(&statements, &setFlag);

	if (!isVoid)
	{
		if (returnStmt->expr.ptr == NULL)
			return ERROR_RESULT("Non-void function must return a value", returnStmt->lineNumber, currentFilePath);

		NodePtr setValue = AllocSetVariable(returnVars->returnValueDecl, CopyASTNode(returnStmt->expr), returnStmt->lineNumber);
		ArrayAdd(&statements, &setValue);
	}
	else
	{
		if (returnStmt->expr.ptr != NULL)
			return ERROR_RESULT("Void function cannot return a value", returnStmt->lineNumber, currentFilePath);
	}

	if (whileVars != NULL)
	{
		NodePtr breakNode = AllocASTNode(
			&(LoopControlStmt){
				.lineNumber = returnStmt->lineNumber,
				.type = LoopControl_Break,
			},
			sizeof(LoopControlStmt), Node_LoopControl);
		PROPAGATE_ERROR(VisitLoopControlStatement(&breakNode, whileVars));
		ArrayAdd(&statements, &breakNode);
	}

	NodePtr new = AllocASTNode(
		&(BlockStmt){
			.lineNumber = returnStmt->lineNumber,
			.statements = statements,
		},
		sizeof(BlockStmt), Node_BlockStatement);

	FreeASTNode(*node);
	*node = new;
	return SUCCESS_RESULT;
}

static Result VisitFunctionBlock(NodePtr blockNode, const Type* returnType);
static Result VisitWhileStatement(NodePtr* whileNode, const ReturnVariables* returnVars, bool isVoid);

static Result VisitBlock(
	NodePtr blockNode,
	const ReturnVariables* returnVars,
	const WhileVariables* whileVars,
	bool isVoid)
{
	ASSERT(blockNode.type == Node_BlockStatement);
	BlockStmt* block = blockNode.ptr;

	for (size_t i = 1; i < block->statements.length; i++)
	{
		if (!StatementReturns(block->statements.array[i - 1], false))
			continue;

		Array statements = AllocateArray(sizeof(NodePtr));
		MoveStatements(block, i, &statements);

		NodePtr ifNode =
			whileVars == NULL
				? AllocIfFlagIsFalse(returnVars->returnFlagDecl, &statements, -1)
				: AllocIfFlagIsFalse(whileVars->continueFlagDecl, &statements, -1);
		ArrayAdd(&block->statements, &ifNode);

		ASSERT(ifNode.type == Node_If);
		ASSERT(((NodePtr*)block->statements.array[i])->ptr == ifNode.ptr);
	}

	for (size_t i = 0; i < block->statements.length; i++)
	{
		NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_Return:
		{
			PROPAGATE_ERROR(VisitReturnStatement(node, returnVars, whileVars, isVoid));
			break;
		}
		case Node_LoopControl:
		{
			PROPAGATE_ERROR(VisitLoopControlStatement(node, whileVars));
			break;
		}
		case Node_BlockStatement:
		{
			PROPAGATE_ERROR(VisitBlock(*node, returnVars, whileVars, isVoid));
			break;
		}
		case Node_If:
		{
			const IfStmt* ifStmt = node->ptr;
			PROPAGATE_ERROR(VisitBlock(ifStmt->trueStmt, returnVars, whileVars, isVoid));
			if (ifStmt->falseStmt.ptr != NULL)
				PROPAGATE_ERROR(VisitBlock(ifStmt->falseStmt, returnVars, whileVars, isVoid));
			break;
		}
		case Node_While:
		{
			PROPAGATE_ERROR(VisitWhileStatement(node, returnVars, isVoid));
			break;
		}
		case Node_FunctionDeclaration:
		{
			const FuncDeclStmt* funcDecl = node->ptr;
			PROPAGATE_ERROR(VisitFunctionBlock(funcDecl->block, &funcDecl->type));
			break;
		}
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}
	}

	return SUCCESS_RESULT;
}

static Result VisitWhileStatement(NodePtr* node, const ReturnVariables* returnVars, bool isVoid)
{
	ASSERT(node->type == Node_While);
	WhileStmt* whileStmt = node->ptr;
	ASSERT(whileStmt->stmt.type == Node_BlockStatement);
	BlockStmt* whileBlock = whileStmt->stmt.ptr;

	BlockStmt* block = AllocASTNode(
		&(BlockStmt){
			.lineNumber = whileStmt->lineNumber,
			.statements = AllocateArray(sizeof(NodePtr)),
		},
		sizeof(BlockStmt), Node_BlockStatement)
						   .ptr; // clang format more like blang blormat
	ArrayAdd(&block->statements, node);
	*node = (NodePtr){.ptr = block, .type = Node_BlockStatement};

	WhileVariables variables = (WhileVariables){
		.breakFlagDecl = AllocFlagDecl(breakFlagName, whileStmt->lineNumber).ptr,
		.continueFlagDecl = AllocFlagDecl(continueFlagName, whileStmt->lineNumber).ptr,
	};
	ArrayInsert(&block->statements, &(NodePtr){.ptr = variables.breakFlagDecl, .type = Node_VariableDeclaration}, 0);
	ArrayInsert(&whileBlock->statements, &(NodePtr){.ptr = variables.continueFlagDecl, .type = Node_VariableDeclaration}, 0);
	whileStmt->expr = AllocBreakFlagExpression(variables.breakFlagDecl, whileStmt->expr, whileStmt->lineNumber);

	return VisitBlock(whileStmt->stmt, returnVars, &variables, isVoid);
}

static bool TypeIsVoid(const Type type)
{
	if (type.expr.type != Node_Literal)
		return false;

	LiteralExpr* literal = type.expr.ptr;
	if (literal->type != Literal_PrimitiveType)
		return false;

	return literal->primitiveType == Primitive_Void;
}

static NodePtr CreateInnerBlock(BlockStmt* block)
{
	NodePtr innerBlock = AllocASTNode(
		&(BlockStmt){
			.lineNumber = block->lineNumber,
			.statements = block->statements,
		},
		sizeof(BlockStmt), Node_BlockStatement);

	block->statements = AllocateArray(sizeof(NodePtr));
	ArrayAdd(&block->statements, &innerBlock);

	return innerBlock;
}

static Result VisitFunctionBlock(NodePtr blockNode, const Type* returnType)
{
	if (blockNode.ptr == NULL)
		return SUCCESS_RESULT;

	ASSERT(blockNode.type == Node_BlockStatement);
	BlockStmt* block = blockNode.ptr;

	NodePtr innerBlock = CreateInnerBlock(block);

	ReturnVariables variables = (ReturnVariables){
		.returnFlagDecl = AllocFlagDecl(returnFlagName, block->lineNumber).ptr,
		.returnValueDecl = NULL,
	};
	ArrayInsert(&block->statements, &(NodePtr){.ptr = variables.returnFlagDecl, .type = Node_VariableDeclaration}, 0);

	const bool isVoid = returnType == NULL || TypeIsVoid(*returnType);
	if (!isVoid)
	{
		if (!StatementReturns(&innerBlock, true))
			return ERROR_RESULT("Not all control paths return a value", block->lineNumber, currentFilePath);

		variables.returnValueDecl = AllocReturnValueDecl(*returnType, block->lineNumber).ptr;
		ArrayInsert(&block->statements, &(NodePtr){.ptr = variables.returnValueDecl, .type = Node_VariableDeclaration}, 0);

		NodePtr returnValue = AllocReturnValueStatement(variables.returnValueDecl, -1);
		ArrayAdd(&block->statements, &returnValue);
	}

	return VisitBlock(innerBlock, &variables, NULL, isVoid);
}

static Result VisitGlobalStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_ExpressionStatement:
	case Node_VariableDeclaration:
	case Node_Import:
	case Node_Input:
	case Node_Null:
		break;
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		PROPAGATE_ERROR(VisitFunctionBlock(section->block, NULL));
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		PROPAGATE_ERROR(VisitFunctionBlock(funcDecl->block, &funcDecl->type));
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; i++)
			PROPAGATE_ERROR(VisitGlobalStatement(block->statements.array[i]));
		break;
	}
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

Result ControlFlowPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitGlobalStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
