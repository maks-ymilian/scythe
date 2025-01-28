#include "ResolverPass.h"

#include <assert.h>
#include <stddef.h>
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
	default: unreachable();
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

static bool GetStructDeclarationFromNode(const NodePtr* node, StructDeclStmt** outStructDecl)
{
	*outStructDecl = NULL;

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
	else if (node->type == Node_ArrayAccess)
	{
		const ArrayAccessExpr* arrayAccess = node->ptr;
		identifier = &arrayAccess->identifier;
	}
	else
		return false;

	NodePtr type = NULL_NODE;

	if (identifier->reference.type == Node_VariableDeclaration)
	{
		const VarDeclStmt* varDecl = identifier->reference.ptr;
		type = varDecl->type;
	}
	else if (identifier->reference.type == Node_FunctionDeclaration)
	{
		const FuncDeclStmt* funcDecl = identifier->reference.ptr;
		type = funcDecl->type;
	}
	else
		return false;

	if (type.type != Node_MemberAccess)
		return false;

	const MemberAccessExpr* last = type.ptr;
	while (last->next.ptr != NULL)
	{
		assert(last->next.type == Node_MemberAccess);
		last = last->next.ptr;
	}

	if (last->value.type != Node_Literal)
		return false;

	const LiteralExpr* literal = last->value.ptr;
	if (literal->type != Literal_Identifier)
		return false;
	if (literal->identifier.reference.type != Node_StructDeclaration)
		return false;

	*outStructDecl = literal->identifier.reference.ptr;
	return true;
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

		StructDeclStmt* structDecl;
		if (GetStructDeclarationFromNode(previous, &structDecl))
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

static Result ResolveExpression(const NodePtr* node, const NodePtr* previous)
{
	switch (node->type)
	{
	case Node_Literal:
	{
		LiteralExpr* literal = node->ptr;

		if (literal->type == Literal_Identifier)
		{
			if (literal->identifier.reference.ptr == NULL)
			{
				PROPAGATE_ERROR(InitializeIdentifierReference(
					&literal->identifier,
					previous,
					literal->lineNumber));

				if (literal->identifier.reference.type == Node_FunctionDeclaration)
					return ERROR_RESULT(
						AllocateString1Str("\"%s\" is a function, not a variable", literal->identifier.text),
						literal->lineNumber,
						currentFilePath);
			}

			return SUCCESS_RESULT;
		}
		else if (literal->type == Literal_Int ||
				 literal->type == Literal_Float ||
				 literal->type == Literal_String ||
				 literal->type == Literal_Boolean ||
				 literal->type == Literal_PrimitiveType)
			return SUCCESS_RESULT;

		unreachable();
	}
	case Node_Binary:
	{
		const BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&binary->left, previous));
		PROPAGATE_ERROR(ResolveExpression(&binary->right, previous));
		return SUCCESS_RESULT;
	}
	case Node_Unary:
	{
		const UnaryExpr* unary = node->ptr;
		return ResolveExpression(&unary->expression, previous);
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* current = node->ptr;
		const NodePtr* previousValue = NULL;
		while (true)
		{
			PROPAGATE_ERROR(ResolveExpression(&current->value, previousValue));

			if (current->next.ptr == NULL)
				break;

			assert(current->next.type == Node_MemberAccess);
			previousValue = &current->value;
			current = current->next.ptr;
		}

		return SUCCESS_RESULT;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;

		if (funcCall->identifier.reference.ptr == NULL)
		{
			PROPAGATE_ERROR(InitializeIdentifierReference(
				&funcCall->identifier,
				previous,
				funcCall->lineNumber));

			if (funcCall->identifier.reference.type != Node_FunctionDeclaration)
				return ERROR_RESULT(
					AllocateString1Str("\"%s\" is not a function", funcCall->identifier.text),
					funcCall->lineNumber,
					currentFilePath);
		}

		for (size_t i = 0; i < funcCall->arguments.length; ++i)
		{
			const NodePtr* node = funcCall->arguments.array[i];
			PROPAGATE_ERROR(ResolveExpression(node, previous));
		}

		return SUCCESS_RESULT;
	}
	case Node_ArrayAccess:
	{
		ArrayAccessExpr* arrayAccess = node->ptr;

		if (arrayAccess->identifier.reference.ptr == NULL)
		{
			PROPAGATE_ERROR(InitializeIdentifierReference(
				&arrayAccess->identifier,
				previous,
				arrayAccess->lineNumber));

			if (arrayAccess->identifier.reference.type != Node_VariableDeclaration ||
				!((VarDeclStmt*)arrayAccess->identifier.reference.ptr)->array)
				return ERROR_RESULT(
					AllocateString1Str("\"%s\" is not an array", arrayAccess->identifier.text),
					arrayAccess->lineNumber,
					currentFilePath);
		}

		PROPAGATE_ERROR(ResolveExpression(&arrayAccess->subscript, previous));

		return SUCCESS_RESULT;
	}
	case Node_Null: return SUCCESS_RESULT;
	default: unreachable();
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
		PROPAGATE_ERROR(ResolveExpression(&varDecl->initializer, NULL));
		PROPAGATE_ERROR(ResolveExpression(&varDecl->arrayLength, NULL));
		PROPAGATE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
		PROPAGATE_ERROR(ResolveExpression(&varDecl->type, NULL));
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
			assert(funcDecl->block.type == Node_Block);
			const BlockStmt* block = funcDecl->block.ptr;
			for (size_t i = 0; i < block->statements.length; ++i)
				PROPAGATE_ERROR(VisitStatement(block->statements.array[i]));
		}
		PopScope(NULL);

		PROPAGATE_ERROR(ResolveExpression(&funcDecl->type, NULL));
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

	case Node_Block: return VisitBlock(node->ptr);
	case Node_ExpressionStatement: return ResolveExpression(&((ExpressionStmt*)node->ptr)->expr, NULL);
	case Node_Return: return ResolveExpression(&((ReturnStmt*)node->ptr)->expr, NULL);

	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		assert(section->block.type == Node_Block);
		return VisitBlock(section->block.ptr);
	}
	case Node_If:
	{
		const IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&ifStmt->expr, NULL));
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
		PROPAGATE_ERROR(ResolveExpression(&whileStmt->expr, NULL));
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
		PROPAGATE_ERROR(ResolveExpression(&forStmt->condition, NULL));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->increment, NULL));
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
	default: unreachable();
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
		unreachable();

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
