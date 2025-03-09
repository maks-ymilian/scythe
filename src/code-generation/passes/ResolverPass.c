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

static bool GetModuleFromNode(const NodePtr* node, Map** outModule, char** outModuleName)
{
	*outModule = NULL;
	*outModuleName = NULL;

	if (node->type != Node_Literal)
		return false;

	const LiteralExpr* literal = node->ptr;
	assert(literal->type == Literal_Identifier);

	if (literal->identifier.reference.type != Node_Import)
		return false;

	const ImportStmt* import = literal->identifier.reference.ptr;
	*outModuleName = import->moduleName;
	assert(*outModuleName);
	*outModule = MapGet(&modules, *outModuleName);
	assert(*outModule);

	return true;
}

static Type* GetTypeFromNode(const NodePtr* node)
{
	const IdentifierReference* identifier = NULL;
	if (node->type == Node_Literal)
	{
		const LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_Identifier);
		identifier = &literal->identifier;
	}
	else if (node->type == Node_FunctionCall)
	{
		const FuncCallExpr* funcCall = node->ptr;
		identifier = &funcCall->identifier;
	}
	else if (node->type == Node_Subscript)
	{
		const SubscriptExpr* subscript = node->ptr;
		identifier = &subscript->identifier;
	}
	else
		return NULL;

	if (identifier->reference.type == Node_VariableDeclaration)
	{
		VarDeclStmt* varDecl = identifier->reference.ptr;
		return &varDecl->type;
	}
	else if (identifier->reference.type == Node_FunctionDeclaration)
	{
		FuncDeclStmt* funcDecl = identifier->reference.ptr;
		return &funcDecl->type;
	}

	return NULL;
}

static StructDeclStmt* GetStructDeclarationFromNode(const NodePtr* node)
{
	Type* type = GetTypeFromNode(node);
	if (type == NULL)
		return NULL;

	if (type->expr.type != Node_MemberAccess)
		return NULL;

	const MemberAccessExpr* last = type->expr.ptr;
	while (last->next.ptr != NULL)
	{
		assert(last->next.type == Node_MemberAccess);
		last = last->next.ptr;
	}

	if (last->value.type != Node_Literal)
		return NULL;

	const LiteralExpr* literal = last->value.ptr;
	if (literal->type != Literal_Identifier)
		return NULL;
	if (literal->identifier.reference.type != Node_StructDeclaration)
		return NULL;

	return literal->identifier.reference.ptr;
}

static Result InitializeIdentifierReference(
	IdentifierReference* identifier,
	const NodePtr* previous,
	const int lineNumber)
{
	if (previous != NULL)
	{
		Map* module;
		char* moduleName;
		if (GetModuleFromNode(previous, &module, &moduleName))
		{
			const NodePtr* node = MapGet(module, identifier->text);
			if (node == NULL)
				return ERROR_RESULT(
					AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", identifier->text, moduleName),
					lineNumber,
					currentFilePath);

			if (!NodeIsPublicDeclaration(node))
				return ERROR_RESULT(
					AllocateString2Str("Declaration \"%s\" in module \"%s\" is private", identifier->text, moduleName),
					lineNumber,
					currentFilePath);

			identifier->reference = *node;
			return SUCCESS_RESULT;
		}

		StructDeclStmt* structDecl = GetStructDeclarationFromNode(previous);
		if (structDecl)
		{
			for (size_t i = 0; i < structDecl->members.length; ++i)
			{
				const NodePtr* node = structDecl->members.array[i];
				if (node->type != Node_VariableDeclaration)
					continue;

				const VarDeclStmt* varDecl = node->ptr;
				if (strcmp(varDecl->name, identifier->text) == 0)
				{
					identifier->reference = *node;
					return SUCCESS_RESULT;
				}
			}

			return ERROR_RESULT(
				AllocateString2Str("Member \"%s\" does not exist in struct \"%s\"", identifier->text, structDecl->name),
				lineNumber,
				currentFilePath);
		}

		Type* type = GetTypeFromNode(previous);
		if (type != NULL && type->array)
		{
			if (strcmp(identifier->text, "offset") != 0 &&
				strcmp(identifier->text, "length") != 0)
				return ERROR_RESULT(
					AllocateString1Str("Member \"%s\" does not exist in array type", identifier->text),
					lineNumber,
					currentFilePath);

			return SUCCESS_RESULT;
		}

		return ERROR_RESULT("Invalid member access", lineNumber, currentFilePath);
	}

	assert(currentScope != NULL);
	const Scope* scope = currentScope;
	while (scope != NULL)
	{
		void* get = MapGet(&scope->declarations, identifier->text);
		if (get != NULL)
		{
			identifier->reference = *(NodePtr*)get;
			return SUCCESS_RESULT;
		}

		scope = scope->parent;
	}

	identifier->reference = NULL_NODE;
	return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", identifier->text), lineNumber, currentFilePath);
}

static Result ResolveExpression(const NodePtr* node);

static Result ResolveMemberAccessValue(const MemberAccessExpr* memberAccess, const NodePtr* previous, bool isType)
{
	IdentifierReference* identifier = NULL;
	int lineNumber = -1;
	switch (memberAccess->value.type)
	{
	case Node_Literal:
		LiteralExpr* literal = memberAccess->value.ptr;
		assert(literal->type == Literal_Identifier);
		identifier = &literal->identifier;
		lineNumber = literal->lineNumber;
		break;
	case Node_Subscript:
		SubscriptExpr* subscript = memberAccess->value.ptr;
		identifier = &subscript->identifier;
		lineNumber = subscript->lineNumber;
		break;
	case Node_FunctionCall:
		FuncCallExpr* funcCall = memberAccess->value.ptr;
		identifier = &funcCall->identifier;
		lineNumber = funcCall->lineNumber;
		break;
	default: INVALID_VALUE(memberAccess->value.type);
	}

	if (identifier->reference.ptr == NULL)
	{
		PROPAGATE_ERROR(InitializeIdentifierReference(identifier, previous, lineNumber));

		if (identifier->reference.type == Node_Import && memberAccess->next.ptr == NULL)
			return ERROR_RESULT("Cannot reference module name by itself", lineNumber, currentFilePath);

		if (memberAccess->value.type == Node_FunctionCall)
		{
			if (identifier->reference.type != Node_FunctionDeclaration)
				return ERROR_RESULT(
					AllocateString1Str("\"%s\" is not a function", identifier->text),
					lineNumber,
					currentFilePath);
		}
		else if (isType)
		{
			if (identifier->reference.type != Node_StructDeclaration &&
				identifier->reference.type != Node_Import)
				return ERROR_RESULT(
					AllocateString1Str("\"%s\" is not a type", identifier->text),
					lineNumber,
					currentFilePath);
		}
		else
		{
			if (identifier->reference.type != Node_VariableDeclaration &&
				identifier->reference.type != Node_Import &&
				identifier->reference.type != Node_Null)
				return ERROR_RESULT(
					AllocateString1Str("\"%s\" is not a variable", identifier->text),
					lineNumber,
					currentFilePath);
		}
	}

	if (memberAccess->value.type == Node_Subscript)
	{
		SubscriptExpr* subscript = memberAccess->value.ptr;
		PROPAGATE_ERROR(ResolveExpression(&subscript->expr));
	}
	else if (memberAccess->value.type == Node_FunctionCall)
	{
		FuncCallExpr* funcCall = memberAccess->value.ptr;
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
		{
			const NodePtr* node = funcCall->arguments.array[i];
			PROPAGATE_ERROR(ResolveExpression(node));
		}
	}
	return SUCCESS_RESULT;
}

static Result ResolveMemberAccess(const NodePtr* node, bool isType)
{
	assert(node->type == Node_MemberAccess);
	const MemberAccessExpr* current = node->ptr;
	const NodePtr* previousValue = NULL;
	while (true)
	{
		PROPAGATE_ERROR(ResolveMemberAccessValue(current, previousValue, isType));

		if (current->next.ptr == NULL)
			break;

		assert(current->next.type == Node_MemberAccess);
		previousValue = &current->value;
		current = current->next.ptr;
	}

	return SUCCESS_RESULT;
}

static Result ResolveType(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_MemberAccess: return ResolveMemberAccess(node, true);
	case Node_Literal: return SUCCESS_RESULT;
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitBlock(const BlockStmt* block);

static Result ResolveExpression(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Literal:
	{
		const LiteralExpr* literal = node->ptr;
		assert(literal->type == Literal_Int ||
			   literal->type == Literal_Float ||
			   literal->type == Literal_String ||
			   literal->type == Literal_Boolean ||
			   literal->type == Literal_PrimitiveType);
		return SUCCESS_RESULT;
	}
	case Node_Binary:
	{
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&binary->left));
		PROPAGATE_ERROR(ResolveExpression(&binary->right));
		return SUCCESS_RESULT;
	}
	case Node_Unary:
	{
		const UnaryExpr* unary = node->ptr;
		return ResolveExpression(&unary->expression);
	}
	case Node_MemberAccess:
	{
		return ResolveMemberAccess(node, false);
	}
	case Node_BlockExpression:
	{
		const BlockExpr* block = node->ptr;
		assert(block->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(ResolveType(&block->type.expr));
		PROPAGATE_ERROR(VisitBlock(block->block.ptr));
		return SUCCESS_RESULT;
	}
	case Node_Null: return SUCCESS_RESULT;
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
		PROPAGATE_ERROR(ResolveExpression(&varDecl->initializer));
		PROPAGATE_ERROR(ResolveType(&varDecl->type.expr));
		PROPAGATE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
		return SUCCESS_RESULT;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;

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

		PROPAGATE_ERROR(ResolveType(&funcDecl->type.expr));
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
	case Node_ExpressionStatement: return ResolveExpression(&((ExpressionStmt*)node->ptr)->expr);
	case Node_Return: return ResolveExpression(&((ReturnStmt*)node->ptr)->expr);

	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		assert(section->block.type == Node_BlockStatement);
		return VisitBlock(section->block.ptr);
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&ifStmt->expr));
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
		PROPAGATE_ERROR(ResolveExpression(&whileStmt->expr));
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
		PROPAGATE_ERROR(ResolveExpression(&forStmt->condition));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->increment));
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
