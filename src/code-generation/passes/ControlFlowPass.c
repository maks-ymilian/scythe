#include "ControlFlowPass.h"

#include "StringUtils.h"

typedef struct
{
	NodePtr hasReturnedDecl;
	NodePtr returnValueDecl;
} ReturnBlockVariables;

static const char* currentFilePath = NULL;

static const char* returnFlagName = "hasReturned";

static NodePtr AllocHasReturned(const int lineNumber)
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

static NodePtr AllocSetReturnFlag(const NodePtr hasReturnedDecl, const int lineNumber)
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
								.reference = hasReturnedDecl,
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

static NodePtr AllocIfReturnFlagIsFalse(const NodePtr hasReturnedDecl, Array* statements)
{
	return AllocASTNode(
		&(IfStmt){
			.lineNumber = -1,
			.falseStmt = NULL_NODE,
			.expr = AllocASTNode(
				&(BinaryExpr){
					.lineNumber = -1,
					.operatorType = Binary_IsEqual,
					.left = AllocASTNode(
						&(LiteralExpr){
							.lineNumber = -1,
							.type = Literal_Identifier,
							.identifier = (IdentifierReference){
								.text = AllocateString(returnFlagName),
								.reference = hasReturnedDecl,
							},
						},
						sizeof(LiteralExpr), Node_Literal),
					.right = AllocASTNode( // clang-format is bad
						&(LiteralExpr){
							.lineNumber = -1,
							.type = Literal_Boolean,
							.boolean = false,
						},
						sizeof(LiteralExpr), Node_Literal),
				},
				sizeof(BinaryExpr), Node_Binary),
			.trueStmt = AllocASTNode( // clang-format is bad
				&(BlockStmt){
					.lineNumber = -1,
					.statements = *statements,
				},
				sizeof(BlockStmt), Node_BlockStatement),
		},
		sizeof(IfStmt), Node_If);
}

static void MoveStatements(BlockStmt* block, size_t startIndex, Array* dest)
{
	for (size_t i = startIndex; i < block->statements.length; i++)
		ArrayAdd(dest, block->statements.array[i]);

	for (size_t i = block->statements.length - 1; i >= startIndex; i--)
		ArrayRemove(&block->statements, i);
}

static Result VisitBlock(BlockStmt* block, const NodePtr returnType, ReturnBlockVariables* variables, bool* outReturns)
{
	if (outReturns != NULL)
		*outReturns = false;

	ReturnBlockVariables _;
	if (variables == NULL)
	{
		variables = &_;
		*variables = (ReturnBlockVariables){
			.hasReturnedDecl = AllocHasReturned(block->lineNumber),
			// todo
			/*.returnValueDecl = ,*/
		},
		ArrayInsert(&block->statements, &variables->hasReturnedDecl, 0);
	}

	bool prevStatementReturns = false;
	for (size_t i = 0; i < block->statements.length; i++)
	{
		if (prevStatementReturns)
		{
			Array statements = AllocateArray(sizeof(NodePtr));
			MoveStatements(block, i, &statements);
			NodePtr ifNode = AllocIfReturnFlagIsFalse(variables->hasReturnedDecl, &statements);
			ArrayAdd(&block->statements, &ifNode);

			assert(((NodePtr*)block->statements.array[i])->ptr == ifNode.ptr);
		}

		NodePtr* node = block->statements.array[i];
		switch (node->type)
		{
		case Node_Return:
			prevStatementReturns = true;

			ReturnStmt* returnStmt = node->ptr;

			Array statements = AllocateArray(sizeof(NodePtr));
			NodePtr setFlag = AllocSetReturnFlag(variables->hasReturnedDecl, returnStmt->lineNumber);
			ArrayAdd(&statements, &setFlag);
			NodePtr new = AllocASTNode(
				&(BlockStmt){
					.lineNumber = returnStmt->lineNumber,
					.statements = statements,
				},
				sizeof(BlockStmt), Node_BlockStatement);

			FreeASTNode(*node);
			*node = new;
			break;
		case Node_BlockStatement:
			BlockStmt* block = node->ptr;
			PROPAGATE_ERROR(VisitBlock(block, NULL_NODE, variables, &prevStatementReturns));
			break;
		case Node_If:
			const IfStmt* ifStmt = node->ptr;

			bool returns;
			assert(ifStmt->trueStmt.type == Node_BlockStatement);
			PROPAGATE_ERROR(VisitBlock(ifStmt->trueStmt.ptr, NULL_NODE, variables, &returns));
			if (returns)
				prevStatementReturns = true;

			if (ifStmt->falseStmt.ptr != NULL)
			{
				assert(ifStmt->falseStmt.type == Node_BlockStatement);
				PROPAGATE_ERROR(VisitBlock(ifStmt->falseStmt.ptr, NULL_NODE, variables, &returns));
				if (returns)
					prevStatementReturns = true;
			}
			break;
		case Node_FunctionDeclaration:
			const FuncDeclStmt* funcDecl = node->ptr;
			assert(funcDecl->block.type == Node_BlockStatement);
			PROPAGATE_ERROR(VisitBlock(funcDecl->block.ptr, funcDecl->type, NULL, NULL));
			break;
		case Node_ExpressionStatement:
		case Node_VariableDeclaration:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
		}

		if (prevStatementReturns && outReturns != NULL)
			*outReturns = true;
	}

	return SUCCESS_RESULT;
}

static Result VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			SectionStmt* section = stmt->ptr;
			assert(section->block.type == Node_BlockStatement);
			PROPAGATE_ERROR(VisitBlock(section->block.ptr, NULL_NODE, NULL, NULL));
			break;
		case Node_Import:
			break;
		default: INVALID_VALUE(stmt->type);
		}
	}

	return SUCCESS_RESULT;
}

Result ControlFlowPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	return SUCCESS_RESULT;
}
