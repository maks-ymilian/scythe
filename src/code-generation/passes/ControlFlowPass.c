#include "ControlFlowPass.h"

#include "StringUtils.h"

typedef struct
{
	NodePtr returnFlagDecl;
	NodePtr returnValueDecl;
} ReturnBlockVariables;

static const char* currentFilePath = NULL;

static const char* returnFlagName = "hasReturned";
static const char* returnValueName = "returnValue";

static NodePtr AllocReturnFlagDecl(const int lineNumber)
{
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = lineNumber,
			.name = AllocateString(returnFlagName),
			.externalName = NULL,
			.arrayLength = NULL,
			.instantiatedVariables = AllocateArray(sizeof(NodePtr)),
			.array = false,
			.public = false,
			.external = false,
			.uniqueName = -1,
			.type = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_PrimitiveType,
					.primitiveType = Primitive_Bool,
				},
				sizeof(LiteralExpr), Node_Literal),
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

static NodePtr AllocSetReturnFlag(const NodePtr returnFlagDecl, const int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = lineNumber,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.left = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(returnFlagName),
								.reference = returnFlagDecl,
							},
						},
						sizeof(LiteralExpr), Node_Literal),
					.right = AllocASTNode( // clang-format is bad
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Boolean,
							.boolean = true,
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

static NodePtr AllocIfReturnFlagIsFalse(const NodePtr returnFlagDecl, Array* statements, const int lineNumber)
{
	return AllocASTNode(
		&(IfStmt){
			.lineNumber = lineNumber,
			.falseStmt = NULL_NODE,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_IsEqual,
					.left = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(returnFlagName),
								.reference = returnFlagDecl,
							},
						},
						sizeof(LiteralExpr), Node_Literal),
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

static NodePtr AllocReturnValueDecl(const NodePtr type, const int lineNumber)
{
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = lineNumber,
			.name = AllocateString(returnValueName),
			.externalName = NULL,
			.arrayLength = NULL,
			.instantiatedVariables = AllocateArray(sizeof(NodePtr)),
			.array = false,
			.public = false,
			.external = false,
			.uniqueName = -1,
			.type = CopyASTNode(type),
			.initializer = NULL_NODE,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
}

static NodePtr AllocSetReturnValue(const NodePtr returnValueDecl, const NodePtr value, const int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = lineNumber,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = lineNumber,
					.operatorType = Binary_Assignment,
					.right = CopyASTNode(value),
					.left = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = lineNumber,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(returnValueName),
								.reference = returnValueDecl,
							},
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
		},
		sizeof(ExpressionStmt), Node_ExpressionStatement);
}

static NodePtr AllocReturnValueStatement(const NodePtr returnValueDecl, const int lineNumber)
{
	return AllocASTNode(
		&(ExpressionStmt){
			.lineNumber = lineNumber,
			.expr = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = lineNumber,
					.type = Literal_Identifier,
					.identifier = (IdentifierReference){
						.text = AllocateString(returnValueName),
						.reference = returnValueDecl,
					},
				},
				sizeof(LiteralExpr), Node_Literal),
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

static bool TypeIsVoid(const NodePtr type)
{
	if (type.type != Node_Literal)
		return false;

	LiteralExpr* literal = type.ptr;
	if (literal->type != Literal_PrimitiveType)
		return false;

	return literal->primitiveType == Primitive_Void;
}

static Result VisitBlock(
	BlockStmt* block,
	const NodePtr returnType,
	ReturnBlockVariables* variables,
	bool* outReturns,
	bool* outAllPathsReturn)
{
	if (outReturns != NULL)
		*outReturns = false;

	const bool isVoid = returnType.ptr == NULL || TypeIsVoid(returnType);
	const bool isTopLevel = variables == NULL;

	ReturnBlockVariables _;
	if (isTopLevel)
	{
		variables = &_;
		variables->returnFlagDecl = AllocReturnFlagDecl(block->lineNumber);
		ArrayInsert(&block->statements, &variables->returnFlagDecl, 0);

		variables->returnValueDecl = NULL_NODE;
		if (!isVoid)
		{
			variables->returnValueDecl = AllocReturnValueDecl(returnType, block->lineNumber);
			ArrayInsert(&block->statements, &variables->returnValueDecl, 0);
		}
	}

	bool allPathsReturn = false;

	bool prevStatementReturns = false;
	for (size_t i = 0; i < block->statements.length; i++)
	{
		IfStmt* checkReturnIf = NULL;
		if (prevStatementReturns)
		{
			Array statements = AllocateArray(sizeof(NodePtr));
			MoveStatements(block, i, &statements);
			NodePtr ifNode = AllocIfReturnFlagIsFalse(variables->returnFlagDecl, &statements, -1);
			ArrayAdd(&block->statements, &ifNode);

			assert(ifNode.type == Node_If);
			checkReturnIf = ifNode.ptr;

			assert(((NodePtr*)block->statements.array[i])->ptr == ifNode.ptr);
		}

		NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_Return:
		{
			prevStatementReturns = true;
			allPathsReturn = true;

			ReturnStmt* returnStmt = node->ptr;

			Array statements = AllocateArray(sizeof(NodePtr));

			NodePtr setFlag = AllocSetReturnFlag(variables->returnFlagDecl, returnStmt->lineNumber);
			ArrayAdd(&statements, &setFlag);

			if (!isVoid)
			{
				if (returnStmt->expr.ptr == NULL)
					return ERROR_RESULT("Non-void function must return a value", returnStmt->lineNumber, currentFilePath);

				NodePtr setValue = AllocSetReturnValue(variables->returnValueDecl, returnStmt->expr, returnStmt->lineNumber);
				ArrayAdd(&statements, &setValue);
			}
			else
			{
				if (returnStmt->expr.ptr != NULL)
					return ERROR_RESULT("Void function cannot return a value", returnStmt->lineNumber, currentFilePath);
			}

			NodePtr new = AllocASTNode(
				&(BlockStmt){
					.lineNumber = returnStmt->lineNumber,
					.statements = statements,
				},
				sizeof(BlockStmt), Node_BlockStatement);

			FreeASTNode(*node);
			*node = new;
			break;
		}
		case Node_BlockStatement:
		{
			BlockStmt* block = node->ptr;
			bool allReturn;
			PROPAGATE_ERROR(VisitBlock(block, returnType, variables, &prevStatementReturns, &allReturn));
			if (allReturn)
				allPathsReturn = true;
			break;
		}
		case Node_If:
		{
			const IfStmt* ifStmt = node->ptr;

			bool returns;
			bool allPathsReturnTrueStmt;
			assert(ifStmt->trueStmt.type == Node_BlockStatement);
			PROPAGATE_ERROR(VisitBlock(ifStmt->trueStmt.ptr, returnType, variables, &returns, &allPathsReturnTrueStmt));
			if (returns)
				prevStatementReturns = true;

			if (ifStmt == checkReturnIf && allPathsReturnTrueStmt)
			{
				allPathsReturn = true;
				break;
			}

			if (ifStmt->falseStmt.ptr != NULL)
			{
				bool allPathsReturnFalseStmt;
				assert(ifStmt->falseStmt.type == Node_BlockStatement);
				PROPAGATE_ERROR(VisitBlock(ifStmt->falseStmt.ptr, returnType, variables, &returns, &allPathsReturnFalseStmt));
				if (returns)
					prevStatementReturns = true;

				if (allPathsReturnTrueStmt && allPathsReturnFalseStmt)
					allPathsReturn = true;
			}
			break;
		}
		case Node_FunctionDeclaration:
		{
			const FuncDeclStmt* funcDecl = node->ptr;
			assert(funcDecl->block.type == Node_BlockStatement);
			PROPAGATE_ERROR(VisitBlock(funcDecl->block.ptr, funcDecl->type, NULL, NULL, NULL));
			break;
		}
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}

		if (prevStatementReturns && outReturns != NULL)
			*outReturns = true;
	}

	if (!isVoid && isTopLevel)
	{
		if (!allPathsReturn)
			return ERROR_RESULT("Not all control paths return a value", block->lineNumber, currentFilePath);

		NodePtr returnValue = AllocReturnValueStatement(variables->returnValueDecl, -1);
		ArrayAdd(&block->statements, &returnValue);
	}

	if (outAllPathsReturn != NULL)
		*outAllPathsReturn = allPathsReturn;

	return SUCCESS_RESULT;
}

static Result VisitGlobalStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Section:
		SectionStmt* section = node->ptr;
		assert(section->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitBlock(section->block.ptr, NULL_NODE, NULL, NULL, NULL));
		break;
	case Node_FunctionDeclaration:
		FuncDeclStmt* funcDecl = node->ptr;
		assert(funcDecl->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(VisitBlock(funcDecl->block.ptr, funcDecl->type, NULL, NULL, NULL));
		break;
	case Node_BlockStatement:
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; i++)
			PROPAGATE_ERROR(VisitGlobalStatement(block->statements.array[i]));
		break;
	case Node_VariableDeclaration:
	case Node_Import:
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
	}

	return SUCCESS_RESULT;
}

Result ControlFlowPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		assert(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		currentFilePath = module->path;

		for (size_t i = 0; i < module->statements.length; ++i)
			PROPAGATE_ERROR(VisitGlobalStatement(module->statements.array[i]));
	}

	return SUCCESS_RESULT;
}
