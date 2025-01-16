#include "DefaultValuePass.h"

#include <assert.h>

static const char* currentFilePath = NULL;

static PrimitiveType GetType(const NodePtr type)
{
	assert(type.type == Node_Literal);
	const LiteralExpr* literal = type.ptr;
	assert(literal->type == Literal_PrimitiveType);
	return literal->primitiveType;
}

static Result VisitVariableDeclaration(VarDeclStmt* varDecl)
{
	if (varDecl->array)
		return SUCCESS_RESULT;

	if (varDecl->initializer.ptr != NULL)
		return SUCCESS_RESULT;

	switch (GetType(varDecl->type))
	{
	case Primitive_Void:
		return ERROR_RESULT("\"void\" is not allowed here", varDecl->lineNumber, currentFilePath);

	case Primitive_Float:
	case Primitive_Int:
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Int,
				.intValue = 0,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;

	case Primitive_Bool:
		varDecl->initializer = AllocASTNode(
			&(LiteralExpr){
				.lineNumber = varDecl->lineNumber,
				.type = Literal_Boolean,
				.boolean = false,
			},
			sizeof(LiteralExpr), Node_Literal);
		break;

	case Primitive_String:
		return ERROR_RESULT("Variables of type \"string\" must have an initializer", varDecl->lineNumber, currentFilePath);

	default: unreachable();
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
		PROPAGATE_ERROR(VisitVariableDeclaration(node->ptr));
		break;
	case Node_Block:
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		break;
	case Node_ExpressionStatement:
	case Node_If:
		break;
	default: unreachable();
	}
	return SUCCESS_RESULT;
}

static Result VisitSection(const SectionStmt* section)
{
	assert(section->block.type == Node_Block);
	const BlockStmt* block = section->block.ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));

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
			PROPAGATE_ERROR(VisitSection(stmt->ptr));
			break;
		case Node_Import:
			break;
		default: unreachable();
		}
	}
	return SUCCESS_RESULT;
}

Result DefaultValuePass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	return SUCCESS_RESULT;
}
