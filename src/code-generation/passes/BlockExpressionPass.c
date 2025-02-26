#include "BlockExpressionPass.h"

#include <assert.h>
#include <stdio.h>

#include "StringUtils.h"

static void VisitExpression(NodePtr* expr, NodePtr* statement, int lineNumber)
{
	switch (expr->type)
	{
		case Node_BlockExpression:
			if (statement->type != Node_BlockStatement)
			{
				Array statements = AllocateArray(sizeof(NodePtr));
				ArrayAdd(&statements, statement);

				*statement = AllocASTNode(
						&(BlockStmt){
							.lineNumber = lineNumber,
							.statements = statements,
						},
						sizeof(BlockStmt), Node_BlockStatement);
			}

			BlockExpr* blockExpr = expr->ptr;
			BlockStmt* blockStmt = statement->ptr;

			const char* name = "block_expression";

			NodePtr funcDecl = AllocASTNode(
					&(FuncDeclStmt){
						.lineNumber = lineNumber,
						.type = blockExpr->type,
						.name = AllocateString(name),
						.externalName = NULL,
						.parameters = AllocateArray(sizeof(NodePtr)), // todo capturing
						.block = blockExpr->block,
						.public = false,
						.external = false,
						.uniqueName = -1,
					},
					sizeof(FuncDeclStmt), Node_FunctionDeclaration);
			ArrayAdd(&blockStmt->statements, &funcDecl);

			blockExpr->type = NULL_NODE;
			blockExpr->block = NULL_NODE;
			FreeASTNode((NodePtr){.ptr = blockExpr, Node_BlockExpression});

			*expr = AllocASTNode(
					&(MemberAccessExpr){
						.next = NULL_NODE,
						.value = AllocASTNode(
								&(FuncCallExpr){
									.lineNumber = lineNumber,
									.arguments = AllocateArray(sizeof(NodePtr)), // todo capturing
									.identifier = (IdentifierReference){
										.text = AllocateString(name),
										.reference = funcDecl,
									}
								},
								sizeof(FuncCallExpr), Node_FunctionCall),
					},
					sizeof(MemberAccessExpr), Node_MemberAccess);
			break;
		case Node_Binary:
			break;
		case Node_Unary:
			break;
		case Node_Literal:
			break;
		case Node_FunctionCall:
			break;
		case Node_ArrayAccess:
			break;
		case Node_MemberAccess:
			break;
		case Node_Null:
			break;
		default: INVALID_VALUE(expr->type);
	}
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
		case Node_BlockStatement:
			const BlockStmt* block = node->ptr;
			for (size_t i = 0; i < block->statements.length; i++)
				VisitStatement(block->statements.array[i]);
			break;
		case Node_If:
			IfStmt* ifStmt = node->ptr;
			// ifStmt->expr todo
			VisitStatement(&ifStmt->trueStmt);
			VisitStatement(&ifStmt->falseStmt);
			break;
		case Node_FunctionDeclaration:
			FuncDeclStmt* funcDecl = node->ptr;
			VisitStatement(&funcDecl->block);
			break;
		case Node_VariableDeclaration:
			VarDeclStmt* varDecl = node->ptr;
			VisitExpression(&varDecl->initializer, node, varDecl->lineNumber);
			VisitExpression(&varDecl->arrayLength, node, varDecl->lineNumber);
			break;
		case Node_ExpressionStatement:
	 		break; // todo
		case Node_StructDeclaration:
			break; // todo
		case Node_Section:
			SectionStmt* section = node->ptr;
			VisitStatement(&section->block);
			break;
		case Node_Import:
		case Node_Null:
			break;
		default: INVALID_VALUE(node->type);
	}
}

static void VisitModule(const ModuleNode* module)
{
	for (size_t i = 0; i < module->statements.length; ++i)
		VisitStatement(module->statements.array[i]);
}

void BlockExpressionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		VisitModule(node->ptr);
	}
}
