#include "TypeConversionPass.h"

#include <StringUtils.h>

static const char* currentFilePath = NULL;

static PrimitiveType GetIdentifierType(const IdentifierReference identifier)
{
	const NodePtr* type;
	switch (identifier.reference.type)
	{
	case Node_VariableDeclaration:
		const VarDeclStmt* varDecl = identifier.reference.ptr;
		type = &varDecl->type;
		break;
	case Node_FunctionDeclaration:
		const FuncDeclStmt* funcDecl = identifier.reference.ptr;
		type = &funcDecl->type;
		break;
	default: unreachable();
	}

	assert(type->type == Node_Literal);
	const LiteralExpr* literal = type->ptr;
	assert(literal->type == Literal_PrimitiveType);
	return literal->primitiveType;
}

static Result VisitLiteral(const LiteralExpr* literal, PrimitiveType* outType)
{
	PrimitiveType _;
	if (outType == NULL)
		outType = &_;

	switch (literal->type)
	{
	case Literal_Identifier:
		if (literal->identifier.reference.type != Node_VariableDeclaration)
			return ERROR_RESULT(
				AllocateString1Str("Identifier \"%s\" is not a variable", literal->identifier.text),
				literal->lineNumber,
				currentFilePath);

		*outType = GetIdentifierType(literal->identifier);
		break;
	case Literal_Float: *outType = Primitive_Float; break;
	case Literal_Int: *outType = Primitive_Int; break;
	case Literal_String: *outType = Primitive_String; break;
	case Literal_Boolean: *outType = Primitive_Bool; break;
	default: unreachable();
	}
	return SUCCESS_RESULT;
}

static Result VisitExpression(const NodePtr* node, PrimitiveType* outType);

static Result VisitBinaryExpression(const NodePtr* node, PrimitiveType* outType)
{
	assert(node->type == Node_Binary);
	const BinaryExpr* binary = node->ptr;

	PROPAGATE_ERROR(VisitExpression(&binary->left, outType));
	PROPAGATE_ERROR(VisitExpression(&binary->right, outType));

	return SUCCESS_RESULT;
}

static Result VisitExpression(const NodePtr* node, PrimitiveType* outType)
{
	switch (node->type)
	{
	case Node_Binary:
		PROPAGATE_ERROR(VisitBinaryExpression(node, outType));
		break;
	case Node_Literal:
		PROPAGATE_ERROR(VisitLiteral(node->ptr, outType));
		break;
	default: unreachable();
	}
	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Null:
		break;
	case Node_FunctionDeclaration:
		const FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			PROPAGATE_ERROR(VisitStatement(funcDecl->parameters.array[i]));
		PROPAGATE_ERROR(VisitStatement(&funcDecl->block));
		break;
	case Node_VariableDeclaration:
		const VarDeclStmt* varDecl = node->ptr;

		PrimitiveType initializerType;
		PROPAGATE_ERROR(VisitExpression(&varDecl->initializer, &initializerType));

		if (varDecl->arrayLength.ptr != NULL)
		{
			PrimitiveType arrayLengthType;
			PROPAGATE_ERROR(VisitExpression(&varDecl->arrayLength, &arrayLengthType));
		}
		break;
	case Node_ExpressionStatement:
		const ExpressionStmt* expressionStmt = node->ptr;
		PROPAGATE_ERROR(VisitExpression(&expressionStmt->expr, NULL));
		break;
	case Node_Block:
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		break;
	case Node_If:
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));

		PrimitiveType type;
		PROPAGATE_ERROR(VisitExpression(&ifStmt->expr, &type));
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

Result TypeConversionPass(const AST* ast)
{
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	return SUCCESS_RESULT;
}
