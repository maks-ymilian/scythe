#include "ResolverPass.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"
#include "data-structures/Map.h"

static const char* currentFilePath = NULL;

static Map modules;

typedef struct Scope Scope;
struct Scope
{
	Scope* parent;
	Map declarations;
};

static Scope* currentScope = NULL;

static void PushScope()
{
	Scope* new = malloc(sizeof(Scope));
	*new = (Scope){
		.declarations = AllocateMap(sizeof(NodePtr)),
		.parent = NULL,
	};

	if (currentScope == NULL)
	{
		currentScope = new;
		return;
	}

	new->parent = currentScope;
	currentScope = new;
}

static void PopScope(Map* outMap)
{
	assert(currentScope != NULL);
	Scope* scope = currentScope;

	currentScope = currentScope->parent;

	if (outMap == NULL)
		FreeMap(&scope->declarations);
	else
		*outMap = scope->declarations;

	free(scope);
}

static Result RegisterDeclaration(const char* name, const NodePtr* node, const int lineNumber)
{
	assert(currentScope != NULL);
	if (!MapAdd(&currentScope->declarations, name, node))
		return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), lineNumber, currentFilePath);
	return SUCCESS_RESULT;
}

static Result ValidateStructAccess(Type type, const char* text, NodePtr* current, int lineNumber)
{
	if (type.expr.type == Node_Literal)
	{
		LiteralExpr* literal = type.expr.ptr;
		assert(literal->type == Literal_PrimitiveType);
		return ERROR_RESULT(
			AllocateString1Str("Type \"%s\" is not an aggregate type",
				GetTokenTypeString(primitiveTypeToTokenType[literal->primitiveType])),
			literal->lineNumber, currentFilePath);
	}

	assert(type.expr.type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = type.expr.ptr;
	assert(memberAccess->typeReference != NULL);
	StructDeclStmt* structDecl = memberAccess->typeReference;
	for (size_t i = 0; i < structDecl->members.length; ++i)
	{
		const NodePtr* node = structDecl->members.array[i];
		if (node->type != Node_VariableDeclaration)
			continue;

		const VarDeclStmt* varDecl = node->ptr;
		if (strcmp(varDecl->name, text) == 0)
		{
			*current = *node;
			return SUCCESS_RESULT;
		}
	}

	return ERROR_RESULT(
		AllocateString2Str("Member \"%s\" does not exist in type \"%s\"", text, structDecl->name),
		lineNumber,
		currentFilePath);
}

static bool NodeIsPublicDeclaration(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration: return ((VarDeclStmt*)node->ptr)->public;
	case Node_FunctionDeclaration: return ((FuncDeclStmt*)node->ptr)->public;
	case Node_StructDeclaration: return ((StructDeclStmt*)node->ptr)->public;
	default: INVALID_VALUE(node->type);
	}
}

static Result ValidateMemberAccess(const char* text, NodePtr* current, int lineNumber)
{
	switch (current->type)
	{
	case Node_Null:
	{
		assert(currentScope != NULL);
		const Scope* scope = currentScope;
		while (scope != NULL)
		{
			void* get = MapGet(&scope->declarations, text);
			if (get != NULL)
			{
				*current = *(NodePtr*)get;
				return SUCCESS_RESULT;
			}

			scope = scope->parent;
		}

		*current = NULL_NODE;
		return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", text), lineNumber, currentFilePath);
	}
	case Node_VariableDeclaration:
	{
		const VarDeclStmt* varDecl = current->ptr;
		return ValidateStructAccess(varDecl->type, text, current, lineNumber);
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = current->ptr;
		assert(funcCall->expr.type == Node_MemberAccess);
		const MemberAccessExpr* memberAccess = funcCall->expr.ptr;
		assert(memberAccess->funcReference != NULL);
		const FuncDeclStmt* funcDecl = memberAccess->funcReference;
		return ValidateStructAccess(funcDecl->type, text, current, lineNumber);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = current->ptr;

		// todo
		switch (subscript->expr.type)
		{
		case Node_MemberAccess:
		{
			const MemberAccessExpr* memberAccess = subscript->expr.ptr;

			assert(memberAccess->varReference != NULL);
			const VarDeclStmt* varDecl = memberAccess->varReference;
			return ValidateStructAccess(varDecl->type, text, current, lineNumber);
		}
		default: INVALID_VALUE(subscript->expr.type);
		}
	}
	case Node_Import:
	{
		const ImportStmt* import = current->ptr;
		Map* module = MapGet(&modules, import->moduleName);
		assert(module != NULL);

		const NodePtr* node = MapGet(module, text);
		if (node == NULL)
			return ERROR_RESULT(
				AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", text, import->moduleName),
				lineNumber,
				currentFilePath);

		if (!NodeIsPublicDeclaration(node))
			return ERROR_RESULT(
				AllocateString2Str("Declaration \"%s\" in module \"%s\" is private", text, import->moduleName),
				lineNumber,
				currentFilePath);

		*current = *node;
		return SUCCESS_RESULT;
	}
	case Node_StructDeclaration:
	case Node_FunctionDeclaration:
		return ERROR_RESULT("Invalid member access", lineNumber, currentFilePath);
	default: INVALID_VALUE(current->type);
	}
}

static NodePtr GetBaseExpression(NodePtr node)
{
	switch (node.type)
	{
	case Node_FunctionCall: return ((FuncCallExpr*)node.ptr)->expr;
	case Node_Subscript: return ((SubscriptExpr*)node.ptr)->expr;
	default: INVALID_VALUE(node.type);
	}
}

static Result ResolveExpression(const NodePtr* node, bool checkForValue);

static Result ResolveMemberAccess(const NodePtr* node)
{
	assert(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	PROPAGATE_ERROR(ResolveExpression(&memberAccess->start, true));

	NodePtr current = memberAccess->start;
	assert(memberAccess->identifiers.array != NULL);
	for (size_t i = 0; i < memberAccess->identifiers.length; ++i)
	{
		char* text = *(char**)memberAccess->identifiers.array[i];
		PROPAGATE_ERROR(ValidateMemberAccess(text, &current, memberAccess->lineNumber));

		if (memberAccess->start.ptr == NULL &&
			current.type == Node_VariableDeclaration &&
			memberAccess->varReference == NULL)
			memberAccess->varReference = current.ptr;
	}

	switch (current.type)
	{
	case Node_FunctionDeclaration:
		memberAccess->funcReference = current.ptr;
		break;
	case Node_StructDeclaration:
		memberAccess->typeReference = current.ptr;
		break;
	case Node_VariableDeclaration:
		assert(memberAccess->varReference != NULL);
		if (memberAccess->varReference != current.ptr)
		{
			memberAccess->parentReference = memberAccess->varReference;
			memberAccess->varReference = current.ptr;
		}
		break;
	default: INVALID_VALUE(current.type);
	}

	return SUCCESS_RESULT;
}

static Result ResolveType(const NodePtr* node, bool voidAllowed)
{
	switch (node->type)
	{
	case Node_MemberAccess:
	{
		PROPAGATE_ERROR(ResolveMemberAccess(node));

		MemberAccessExpr* memberAccess = node->ptr;
		if (memberAccess->typeReference == NULL)
			return ERROR_RESULT("Expression is not a type", memberAccess->lineNumber, currentFilePath);

		return SUCCESS_RESULT;
	}
	case Node_Literal:
	{
		if (voidAllowed)
			return SUCCESS_RESULT;

		LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_PrimitiveType);
		if (literal->primitiveType == Primitive_Void)
			return ERROR_RESULT("\"void\" is not allowed here", literal->lineNumber, currentFilePath);

		return SUCCESS_RESULT;
	}
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitBlock(const BlockStmt* block);

static Result ResolveExpression(const NodePtr* node, bool checkForValue)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		return SUCCESS_RESULT;
	case Node_MemberAccess:
	{
		PROPAGATE_ERROR(ResolveMemberAccess(node));
		if (checkForValue)
		{
			const MemberAccessExpr* memberAccess = node->ptr;
			if (memberAccess->varReference == NULL)
				return ERROR_RESULT("Expression is not a variable or value", memberAccess->lineNumber, currentFilePath);
		}
		return SUCCESS_RESULT;
	}
	case Node_Binary:
	{
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&binary->left, true));
		PROPAGATE_ERROR(ResolveExpression(&binary->right, true));
		return SUCCESS_RESULT;
	}
	case Node_Unary:
	{
		const UnaryExpr* unary = node->ptr;
		return ResolveExpression(&unary->expression, true);
	}
	case Node_BlockExpression:
	{
		const BlockExpr* block = node->ptr;
		assert(block->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(ResolveType(&block->type.expr, false));
		PROPAGATE_ERROR(VisitBlock(block->block.ptr));
		return SUCCESS_RESULT;
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* funcCall = node->ptr;

		PROPAGATE_ERROR(ResolveExpression(&funcCall->expr, false));
		if (funcCall->expr.type == Node_MemberAccess)
		{
			MemberAccessExpr* memberAccess = funcCall->expr.ptr;
			if (memberAccess->funcReference == NULL ||
				memberAccess->start.ptr != NULL)
				goto notFunctionError;
		}
		else
			goto notFunctionError;

		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			PROPAGATE_ERROR(ResolveExpression(funcCall->arguments.array[i], true));

		return SUCCESS_RESULT;

	notFunctionError:
		return ERROR_RESULT("Expression is not a function", funcCall->lineNumber, currentFilePath);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&subscript->expr, true));
		PROPAGATE_ERROR(ResolveExpression(&subscript->indexExpr, true));
		return SUCCESS_RESULT;
	}
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitStatement(const NodePtr* node);

static Result VisitBlock(const BlockStmt* block)
{
	PushScope();
	for (size_t i = 0; i < block->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
	PopScope(NULL);

	return SUCCESS_RESULT;
}

static Result RecursiveRegisterImportNode(const NodePtr* importNode, const bool topLevel)
{
	assert(importNode->type == Node_Import);
	const ImportStmt* import = importNode->ptr;

	const Map* module = MapGet(&modules, import->moduleName);
	assert(module != NULL);

	PROPAGATE_ERROR(RegisterDeclaration(import->moduleName, importNode, import->lineNumber));

	if (import->public || topLevel)
	{
		for (MAP_ITERATE(i, module))
		{
			const NodePtr* node = i->value;
			if (node->type == Node_Import &&
				((ImportStmt*)node->ptr)->public)
				RecursiveRegisterImportNode(node, false);
		}
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&varDecl->initializer, true));
		PROPAGATE_ERROR(ResolveType(&varDecl->type.expr, false));
		PROPAGATE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
		return SUCCESS_RESULT;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;

		PROPAGATE_ERROR(ResolveType(&funcDecl->type.expr, true));

		PushScope();
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			PROPAGATE_ERROR(VisitStatement(funcDecl->parameters.array[i]));

		if (funcDecl->block.ptr != NULL)
		{
			assert(funcDecl->block.type == Node_BlockStatement);
			const BlockStmt* block = funcDecl->block.ptr;
			for (size_t i = 0; i < block->statements.length; ++i)
				PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		}
		PopScope(NULL);

		PROPAGATE_ERROR(RegisterDeclaration(funcDecl->name, node, funcDecl->lineNumber));
		return SUCCESS_RESULT;
	}
	case Node_StructDeclaration:
	{
		const StructDeclStmt* structDecl = node->ptr;

		PushScope();
		for (size_t i = 0; i < structDecl->members.length; ++i)
			PROPAGATE_ERROR(VisitStatement(structDecl->members.array[i]));
		PopScope(NULL);

		PROPAGATE_ERROR(RegisterDeclaration(structDecl->name, node, structDecl->lineNumber));
		return SUCCESS_RESULT;
	}

	case Node_BlockStatement: return VisitBlock(node->ptr);
	case Node_ExpressionStatement: return ResolveExpression(&((ExpressionStmt*)node->ptr)->expr, true);
	case Node_Return: return ResolveExpression(&((ReturnStmt*)node->ptr)->expr, true);

	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		assert(section->block.type == Node_BlockStatement);
		return VisitBlock(section->block.ptr);
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&ifStmt->expr, true));
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&ifStmt->trueStmt));
		PopScope(NULL);
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&ifStmt->falseStmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_While:
	{
		const WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&whileStmt->expr, true));
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_For:
	{
		const ForStmt* forStmt = node->ptr;
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&forStmt->initialization));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->condition, true));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->increment, true));
		PROPAGATE_ERROR(VisitStatement(&forStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}

	case Node_Import:
	{
		PROPAGATE_ERROR(RecursiveRegisterImportNode(node, true));
		return SUCCESS_RESULT;
	}
	case Node_LoopControl:
	case Node_Null:
		return SUCCESS_RESULT;
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitModule(const ModuleNode* module)
{
	currentFilePath = module->path;

	PushScope();
	for (size_t i = 0; i < module->statements.length; ++i)
		PROPAGATE_ERROR(VisitStatement(module->statements.array[i]));

	Map declarations;
	PopScope(&declarations);
	if (!MapAdd(&modules, module->moduleName, &declarations))
		assert(0);

	return SUCCESS_RESULT;
}

Result ResolverPass(const AST* ast)
{
	modules = AllocateMap(sizeof(Map));
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	for (MAP_ITERATE(i, &modules))
		FreeMap(i->value);
	FreeMap(&modules);

	return SUCCESS_RESULT;
}
