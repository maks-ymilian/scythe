#include "ForLoopPass.h"

static void VisitStatement(NodePtr* node);

static void VisitForStatement(NodePtr* node)
{
	ASSERT(node->type == Node_For);
	ForStmt* forStmt = node->ptr;

	VisitStatement(&forStmt->stmt);

	if (forStmt->condition.ptr == NULL)
	{
		forStmt->condition = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = forStmt->lineNumber,
				.type = Literal_Boolean,
				.boolean = true,
			},
			sizeof(LiteralExpr), Node_Literal);
	}

	ASSERT(forStmt->stmt.type == Node_BlockStatement);
	BlockStmt* whileBlock = forStmt->stmt.ptr;
	forStmt->stmt = NULL_NODE;

	NodePtr whileStmt = AllocASTNode(
		&(WhileStmt){
			.lineNumber = forStmt->lineNumber,
			.expr = forStmt->condition,
			.stmt = (NodePtr){.ptr = whileBlock, .type = Node_BlockStatement},
		},
		sizeof(WhileStmt), Node_While);
	forStmt->condition = NULL_NODE;

	BlockStmt* outerBlock = AllocASTNode(
		&(BlockStmt){
			.lineNumber = forStmt->lineNumber,
			.statements = AllocateArray(sizeof(NodePtr)),
		},
		sizeof(BlockStmt), Node_BlockStatement)
								.ptr;
	ArrayAdd(&outerBlock->statements, &whileStmt);
	*node = (NodePtr){.ptr = outerBlock, .type = Node_BlockStatement};

	if (forStmt->initialization.ptr != NULL)
	{
		ArrayInsert(&outerBlock->statements, &forStmt->initialization, 0);
		forStmt->initialization = NULL_NODE;
	}

	if (forStmt->increment.ptr != NULL)
	{
		ASSERT(forStmt->increment.type != Node_ExpressionStatement);
		NodePtr node = AllocASTNode(
			&(ExpressionStmt){
				.lineNumber = forStmt->lineNumber,
				.expr = forStmt->increment,
			},
			sizeof(ExpressionStmt), Node_ExpressionStatement);
		ArrayAdd(&whileBlock->statements, &node);
		forStmt->increment = NULL_NODE;
	}

	FreeASTNode((NodePtr){.ptr = forStmt, .type = Node_For});
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Return:
	case Node_VariableDeclaration:
	case Node_StructDeclaration:
	case Node_ExpressionStatement:
	case Node_LoopControl:
	case Node_Import:
	case Node_Input:
	case Node_Desc:
	case Node_Null:
		break;
	case Node_For:
	{
		VisitForStatement(node);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		VisitStatement(&funcDecl->block);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		VisitStatement(&section->block);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		VisitStatement(&whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void ForLoopPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);
	}
}
