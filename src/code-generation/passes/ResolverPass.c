#include "ResolverPass.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "StringUtils.h"
#include "data-structures/Map.h"

typedef struct Scope Scope;
struct Scope
{
	Scope* parent;
	Map declarations;
};

static const char* currentFilePath;

static Map modules;

static Scope* currentScope;

static ModuleNode* currentModule;
static Map structTypeToArrayStruct;
static StructDeclStmt* primitiveTypeToArrayStruct[] = {
	[Primitive_Any] = NULL,
	[Primitive_Float] = NULL,
	[Primitive_Int] = NULL,
	[Primitive_Bool] = NULL,
};

static ModifierState currentModifierState;

static Result VisitStatement(NodePtr* node);
static Result ResolveExpression(NodePtr* node, bool checkForValue, FuncCallExpr* resolveFuncCall);
static Result ResolveType(Type* type, bool voidAllowed, bool* isType);
static Result VisitBlock(const BlockStmt* block);
static Result FindFunctionOverloadScoped(const char* text, NodePtr* outNode, size_t argCount, bool* isAmbiguous, int lineNumber);

static NodePtr AllocMemberVarDecl(const char* name, Type type)
{
	return AllocASTNode(
		&(VarDeclStmt){
			.lineNumber = -1,
			.type = type,
			.name = AllocateString(name),
			.initializer = NULL_NODE,
			.instantiatedVariables = (Array){.array = NULL},
			.uniqueName = -1,
		},
		sizeof(VarDeclStmt), Node_VariableDeclaration);
}

static StructDeclStmt* GetSetArrayStructDecl(Type type, StructDeclStmt* structDecl)
{
	switch (type.expr.type)
	{
	case Node_Literal:
	{
		LiteralExpr* literal = type.expr.ptr;
		assert(literal->type == Literal_PrimitiveType);

		if (structDecl)
		{
			assert(primitiveTypeToArrayStruct[literal->primitiveType] == NULL);
			primitiveTypeToArrayStruct[literal->primitiveType] = structDecl;
			return structDecl;
		}
		else
			return primitiveTypeToArrayStruct[literal->primitiveType];
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = type.expr.ptr;
		assert(memberAccess->typeReference != NULL);

		char lookup[64];
		snprintf(lookup, sizeof(lookup), "%p", (void*)memberAccess->typeReference);

		if (structDecl)
		{
			bool success = MapAdd(&structTypeToArrayStruct, lookup, &structDecl);
			assert(success);
			return structDecl;
		}
		else
		{
			StructDeclStmt** get = MapGet(&structTypeToArrayStruct, lookup);
			return get ? *get : NULL;
		}
	}
	default: INVALID_VALUE(type.expr.type);
	}
}

static StructDeclStmt* CreateOrGetArrayStructDecl(Type type)
{
	// find struct
	StructDeclStmt* structDecl = GetSetArrayStructDecl(type, NULL);
	if (structDecl)
		return structDecl;

	// create struct
	structDecl = AllocASTNode(
		&(StructDeclStmt){
			.lineNumber = -1,
			.name = NULL,
			.members = AllocateArray(sizeof(NodePtr)),
			.isArrayType = true,
		},
		sizeof(StructDeclStmt), Node_StructDeclaration)
					 .ptr;

	NodePtr ptrMember = AllocMemberVarDecl("ptr",
		(Type){
			.modifier = TypeModifier_Pointer,
			.expr = CopyASTNode(type.expr),
		});
	ArrayInsert(&structDecl->members, &ptrMember, ARRAY_STRUCT_PTR_MEMBER_INDEX);

	NodePtr lengthMember = AllocMemberVarDecl("length",
		(Type){
			.modifier = TypeModifier_None,
			.expr = AllocASTNode(
				&(LiteralExpr){
					.lineNumber = -1,
					.type = Literal_PrimitiveType,
					.primitiveType = Primitive_Int,
				},
				sizeof(LiteralExpr), Node_Literal),
		});
	ArrayInsert(&structDecl->members, &lengthMember, ARRAY_STRUCT_LENGTH_MEMBER_INDEX);

	assert(structDecl->members.length == ARRAY_STRUCT_MEMBER_COUNT);

	assert(currentModule != NULL);
	ArrayAdd(&currentModule->statements, &(NodePtr){.ptr = structDecl, .type = Node_StructDeclaration});

	// add so it can be found
	GetSetArrayStructDecl(type, structDecl);

	return structDecl;
}

static void ChangeArrayTypeToStruct(Type* type)
{
	if (type->modifier != TypeModifier_Array)
		return;

	// if its void just return
	if (type->expr.type == Node_Literal)
	{
		LiteralExpr* literal = type->expr.ptr;
		assert(literal->type == Literal_PrimitiveType);
		if (literal->primitiveType == Primitive_Void)
			return;
	}

	StructDeclStmt* typeReference = CreateOrGetArrayStructDecl(*type);

	NodePtr oldExpr = type->expr;
	*type = (Type){
		.modifier = TypeModifier_None,
		.expr = AllocASTNode(
			&(MemberAccessExpr){
				.lineNumber = -1,
				.start = NULL_NODE,
				.identifiers = (Array){.array = NULL},
				.funcReference = NULL,
				.typeReference = typeReference,
				.varReference = NULL,
				.parentReference = NULL,
			},
			sizeof(MemberAccessExpr), Node_MemberAccess),
	};
	FreeASTNode(oldExpr);
}

static void FreeDeclarations(Map* declarations)
{
	for (MAP_ITERATE(i, declarations))
	{
		Array* array = i->value;
		FreeArray(array);
	}
	FreeMap(declarations);
}

static NodePtr* GetFirstNode(const Map* declarations, const char* key)
{
	Array* array = MapGet(declarations, key);
	if (array == NULL)
		return NULL;
	else
	{
		assert(array->length >= 1);
		return (NodePtr*)array->array[0];
	}
}

static void PushScope()
{
	Scope* new = malloc(sizeof(Scope));
	*new = (Scope){
		.declarations = AllocateMap(sizeof(Array)),
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
		FreeDeclarations(&scope->declarations);
	else
		*outMap = scope->declarations;

	free(scope);
}

static bool VariadicFunctionIsAmbiguous(const char* name, FuncDeclStmt* funcDecl)
{
	assert(currentScope != NULL);
	const Scope* scope = currentScope;
	for (; scope != NULL; scope = scope->parent)
	{
		Array* array = MapGet(&scope->declarations, name);
		if (array == NULL)
			continue;

		assert(array->length >= 1);
		for (size_t i = 0; i < array->length; ++i)
		{
			NodePtr* node = array->array[i];
			assert(node->type == Node_FunctionDeclaration);
			FuncDeclStmt* currentFunc = node->ptr;

			if (currentFunc == funcDecl)
				continue;

			if (currentFunc->variadic || currentFunc->parameters.length >= funcDecl->parameters.length)
				return true;
		}
	}

	return false;
}

static Result RegisterDeclaration(const char* name, const NodePtr* node, const int lineNumber)
{
	assert(currentScope != NULL);
	assert(node != NULL);

	Array* array = MapGet(&currentScope->declarations, name);
	if (array)
	{
		if (node->type != Node_FunctionDeclaration)
			return ERROR_RESULT(AllocateString1Str("\"%s\" is already defined", name), lineNumber, currentFilePath);

		ArrayAdd(array, node);
	}
	else
	{
		Array array = AllocateArray(sizeof(NodePtr));
		ArrayAdd(&array, node);

		if (!MapAdd(&currentScope->declarations, name, &array))
			assert(0);
	}

	// check for function overload ambiguity
	if (node->type == Node_FunctionDeclaration)
	{
		FuncDeclStmt* funcDecl = node->ptr;
		bool isAmbiguous = false;

		if (funcDecl->variadic)
			isAmbiguous = VariadicFunctionIsAmbiguous(name, funcDecl);
		else
			PROPAGATE_ERROR(FindFunctionOverloadScoped(name, NULL, funcDecl->parameters.length, &isAmbiguous, lineNumber));

		if (isAmbiguous)
			return ERROR_RESULT(
				AllocateString1Str(
					"Function declaration \"%s\" is ambiguous with another overload",
					funcDecl->name),
				lineNumber,
				currentFilePath);
	}

	return SUCCESS_RESULT;
}

static Result ValidateStructAccess(Type type, const char* text, NodePtr* current, int lineNumber)
{
	if (type.modifier == TypeModifier_Pointer)
		return ERROR_RESULT("Cannot access members in pointer type", lineNumber, currentFilePath);

	if (type.expr.type == Node_Literal)
	{
		LiteralExpr* literal = type.expr.ptr;
		assert(literal->type == Literal_PrimitiveType);
		return ERROR_RESULT(
			AllocateString1Str("Type \"%s\" is not an aggregate type",
				GetTokenTypeString(primitiveTypeToTokenType[literal->primitiveType])),
			lineNumber, currentFilePath);
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

	if (structDecl->isArrayType)
		return ERROR_RESULT(
			AllocateString1Str("Member \"%s\" does not exist in array type", text),
			lineNumber,
			currentFilePath);
	else
		return ERROR_RESULT(
			AllocateString2Str("Member \"%s\" does not exist in type \"%s\"", text, structDecl->name),
			lineNumber,
			currentFilePath);
}

static bool NodeIsPublicDeclaration(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration: return ((VarDeclStmt*)node->ptr)->modifiers.publicValue;
	case Node_FunctionDeclaration: return ((FuncDeclStmt*)node->ptr)->modifiers.publicValue;
	case Node_StructDeclaration: return ((StructDeclStmt*)node->ptr)->modifiers.publicValue;
	default: INVALID_VALUE(node->type);
	}
}

static size_t Max(size_t a, size_t b)
{
	return a > b ? a : b;
}

static bool CheckFunctionOverload(size_t argCount, size_t paramCount, bool isVariadic)
{
	for (size_t i = 0; i < Max(argCount, paramCount); ++i)
	{
		if ((!isVariadic && i < argCount && i >= paramCount) ||
			(i >= argCount && i < paramCount))
			return false;
	}

	return true;
}

static bool FindFunctionOverload(const char* name, const Map* declarations, size_t argCount, bool* isAmbiguous, NodePtr* out)
{
	Array* array = MapGet(declarations, name);
	if (array == NULL)
		return false;

	assert(array->length >= 1);
	for (size_t i = 0; i < array->length; ++i)
	{
		NodePtr* node = array->array[i];
		assert(node->type == Node_FunctionDeclaration);
		FuncDeclStmt* funcDecl = node->ptr;

		if (CheckFunctionOverload(argCount, funcDecl->parameters.length, funcDecl->variadic))
		{
			if (out->ptr && isAmbiguous)
				*isAmbiguous = true;

			*out = *node;
		}
	}

	return true;
}

static Result FindFunctionOverloadScoped(const char* name, NodePtr* out, size_t argCount, bool* isAmbiguous, int lineNumber)
{
	NodePtr _;
	if (!out)
		out = &_;

	if (isAmbiguous)
		*isAmbiguous = false;

	*out = NULL_NODE;

	bool functionNameExists = false;
	assert(currentScope != NULL);
	const Scope* scope = currentScope;
	for (; scope != NULL; scope = scope->parent)
		functionNameExists = FindFunctionOverload(name, &scope->declarations, argCount, isAmbiguous, out);

	if (out->ptr)
		return SUCCESS_RESULT;
	else if (functionNameExists)
		return ERROR_RESULT(
			AllocateString1Str1Int(
				"Could not find overload for function \"%s\" with %d parameter(s)",
				name,
				argCount),
			lineNumber,
			currentFilePath);
	else
		return ERROR_RESULT(AllocateString1Str("Unknown identifier \"%s\"", name), lineNumber, currentFilePath);
}

static Result ValidateMemberAccess(const char* text, NodePtr* current, FuncCallExpr* resolveFuncCall, int lineNumber)
{
	switch (current->type)
	{
	case Node_Null:
	{
		assert(currentScope != NULL);

		const Scope* scope = currentScope;
		for (; scope != NULL; scope = scope->parent)
		{
			NodePtr* node = GetFirstNode(&scope->declarations, text);
			if (node != NULL)
			{
				// if resolving the baseExpr of a FuncCallExpr
				if (resolveFuncCall && node->type == Node_FunctionDeclaration)
					return FindFunctionOverloadScoped(text, current, resolveFuncCall->arguments.length, NULL, lineNumber);

				*current = *node;
				return SUCCESS_RESULT;
			}
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
		assert(funcCall->baseExpr.type == Node_MemberAccess);
		const MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
		assert(memberAccess->funcReference != NULL);
		const FuncDeclStmt* funcDecl = memberAccess->funcReference;
		return ValidateStructAccess(funcDecl->type, text, current, lineNumber);
	}
	case Node_Subscript:
	{
		const SubscriptExpr* subscript = current->ptr;

		// todo
		switch (subscript->baseExpr.type)
		{
		case Node_MemberAccess:
		{
			const MemberAccessExpr* memberAccess = subscript->baseExpr.ptr;

			assert(memberAccess->varReference != NULL);
			const VarDeclStmt* varDecl = memberAccess->varReference;

			// dereference
			Type type = varDecl->type;
			if (type.modifier == TypeModifier_Pointer) // pointer
				type.modifier = TypeModifier_None;
			else if (type.expr.type == Node_MemberAccess)
			{
				StructDeclStmt* structDecl = ((MemberAccessExpr*)type.expr.ptr)->typeReference;
				assert(structDecl);
				if (structDecl->isArrayType) // array
				{
					type = GetPtrMember(structDecl)->type;
					assert(type.modifier == TypeModifier_Pointer);
					type.modifier = TypeModifier_None;
				}
			}

			return ValidateStructAccess(type, text, current, lineNumber);
		}
		default: INVALID_VALUE(subscript->baseExpr.type);
		}
	}
	case Node_Import:
	{
		const ImportStmt* import = current->ptr;
		Map* declarations = MapGet(&modules, import->moduleName);
		assert(declarations != NULL);

		NodePtr* first = GetFirstNode(declarations, text);
		if (first == NULL)
			return ERROR_RESULT(
				AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", text, import->moduleName),
				lineNumber,
				currentFilePath);

		NodePtr node = *first;

		// if resolving the baseExpr of a FuncCallExpr
		if (resolveFuncCall)
		{
			assert(node.type == Node_FunctionDeclaration);

			node = NULL_NODE;
			bool exists = FindFunctionOverload(text, declarations, resolveFuncCall->arguments.length, NULL, &node);

			if (!node.ptr)
			{
				if (exists)
					return ERROR_RESULT(
						AllocateString1Str1Int(
							"Could not find overload for function \"%s\" with %d parameter(s)",
							text,
							resolveFuncCall->arguments.length),
						lineNumber,
						currentFilePath);
				else
					return ERROR_RESULT(
						AllocateString2Str("Unknown identifier \"%s\" in module \"%s\"", text, import->moduleName),
						lineNumber,
						currentFilePath);
			}
		}

		if (!NodeIsPublicDeclaration(&node))
			return ERROR_RESULT(
				AllocateString2Str("Declaration \"%s\" in module \"%s\" is private", text, import->moduleName),
				lineNumber,
				currentFilePath);

		*current = node;
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
	case Node_FunctionCall: return ((FuncCallExpr*)node.ptr)->baseExpr;
	case Node_Subscript: return ((SubscriptExpr*)node.ptr)->baseExpr;
	default: INVALID_VALUE(node.type);
	}
}

static Result ResolveMemberAccess(NodePtr* node, FuncCallExpr* resolveFuncCall)
{
	assert(node->type == Node_MemberAccess);
	MemberAccessExpr* memberAccess = node->ptr;

	PROPAGATE_ERROR(ResolveExpression(&memberAccess->start, true, NULL));

	NodePtr current = memberAccess->start;
	assert(memberAccess->identifiers.array != NULL);
	for (size_t i = 0; i < memberAccess->identifiers.length; ++i)
	{
		char* text = *(char**)memberAccess->identifiers.array[i];
		PROPAGATE_ERROR(ValidateMemberAccess(text, &current, resolveFuncCall, memberAccess->lineNumber));

		// set varReference to the first one for now
		if (current.type == Node_VariableDeclaration &&
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
		// if there is more identifiers after varReference then change it to parent
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

static Result ResolveType(Type* type, bool voidAllowed, bool* outIsType)
{
	if (outIsType)
		*outIsType = true;

	switch (type->expr.type)
	{
	case Node_MemberAccess:
	{
		PROPAGATE_ERROR(ResolveMemberAccess(&type->expr, NULL));

		assert(type->expr.type == Node_MemberAccess);
		MemberAccessExpr* memberAccess = type->expr.ptr;
		if (memberAccess->typeReference == NULL)
		{
			if (outIsType)
			{
				*outIsType = false;
				return SUCCESS_RESULT;
			}
			else
				return ERROR_RESULT("Expression is not a type", memberAccess->lineNumber, currentFilePath);
		}

		break;
	}
	case Node_Literal:
	{
		if (voidAllowed)
			break;

		LiteralExpr* literal = type->expr.ptr;
		assert(literal->type == Literal_PrimitiveType);
		if (literal->primitiveType == Primitive_Void)
			return ERROR_RESULT("\"void\" is not allowed here", literal->lineNumber, currentFilePath);

		break;
	}
	default: INVALID_VALUE(type->expr.type);
	}

	ChangeArrayTypeToStruct(type);
	return SUCCESS_RESULT;
}

static Result ResolveExpression(NodePtr* node, bool checkForValue, FuncCallExpr* resolveFuncCall)
{
	switch (node->type)
	{
	case Node_Literal:
	case Node_Null:
		return SUCCESS_RESULT;
	case Node_MemberAccess:
	{
		PROPAGATE_ERROR(ResolveMemberAccess(node, resolveFuncCall));
		if (node->type == Node_MemberAccess && checkForValue)
		{
			const MemberAccessExpr* memberAccess = node->ptr;
			if (memberAccess->varReference == NULL)
				return ERROR_RESULT("Expression is not a variable or value", memberAccess->lineNumber, currentFilePath);
		}
		return SUCCESS_RESULT;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&binary->left, true, NULL));
		PROPAGATE_ERROR(ResolveExpression(&binary->right, true, NULL));
		return SUCCESS_RESULT;
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node->ptr;

		// syntax sugar for dereferencing
		if (unary->operatorType == Unary_Dereference)
		{
			NodePtr old = *node;
			*node = AllocASTNode(
				&(SubscriptExpr){
					.lineNumber = unary->lineNumber,
					.baseExpr = unary->expression,
					.indexExpr = AllocInteger(0, unary->lineNumber),
				},
				sizeof(SubscriptExpr), Node_Subscript);
			unary->expression = NULL_NODE;
			FreeASTNode(old);

			return ResolveExpression(node, checkForValue, NULL);
		}

		return ResolveExpression(&unary->expression, true, NULL);
	}
	case Node_BlockExpression:
	{
		BlockExpr* block = node->ptr;
		assert(block->block.type == Node_BlockStatement);
		PROPAGATE_ERROR(ResolveType(&block->type, true, NULL));
		PROPAGATE_ERROR(VisitBlock(block->block.ptr));
		return SUCCESS_RESULT;
	}
	case Node_SizeOf:
	{
		SizeOfExpr* sizeOf = node->ptr;
		bool isType = false;
		PROPAGATE_ERROR(ResolveType(&sizeOf->type, false, &isType));
		if (isType)
		{
			FreeASTNode(sizeOf->expr);
			sizeOf->expr = NULL_NODE;
		}
		else
		{
			FreeASTNode(sizeOf->type.expr);
			sizeOf->type = (Type){.expr = NULL_NODE};
			PROPAGATE_ERROR(ResolveExpression(&sizeOf->expr, true, NULL));
		}
		return SUCCESS_RESULT;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;

		PROPAGATE_ERROR(ResolveExpression(&funcCall->baseExpr, false, funcCall));
		if (funcCall->baseExpr.type == Node_MemberAccess)
		{
			MemberAccessExpr* memberAccess = funcCall->baseExpr.ptr;
			if (memberAccess->funcReference == NULL ||
				memberAccess->start.ptr != NULL)
				goto notFunctionError;
		}
		else
			goto notFunctionError;

		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			PROPAGATE_ERROR(ResolveExpression(funcCall->arguments.array[i], true, NULL));

		return SUCCESS_RESULT;

	notFunctionError:
		return ERROR_RESULT("Expression is not a function", funcCall->lineNumber, currentFilePath);
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&subscript->baseExpr, true, NULL));
		PROPAGATE_ERROR(ResolveExpression(&subscript->indexExpr, true, NULL));
		return SUCCESS_RESULT;
	}
	default: INVALID_VALUE(node->type);
	}
}

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

	const Map* declarations = MapGet(&modules, import->moduleName);
	assert(declarations != NULL);

	PROPAGATE_ERROR(RegisterDeclaration(import->moduleName, importNode, import->lineNumber));

	if (import->modifiers.publicValue || topLevel)
	{
		for (MAP_ITERATE(i, declarations))
		{
			Array* array = i->value;
			assert(array->length >= 1);

			NodePtr* node = array->array[0];
			if (node->type == Node_Import &&
				((ImportStmt*)node->ptr)->modifiers.publicValue)
				RecursiveRegisterImportNode(node, false);
		}
	}

	return SUCCESS_RESULT;
}

static Result SetModifiers(ModifierState* modifiers, int lineNumber)
{
	if (!currentScope->parent) // global scope
	{
		if (currentModifierState.publicSpecified && !modifiers->publicSpecified)
			modifiers->publicValue = currentModifierState.publicValue;
		if (currentModifierState.externalSpecified && !modifiers->externalSpecified)
			modifiers->externalValue = currentModifierState.externalValue;
	}
	else
	{
		if (modifiers->publicSpecified || modifiers->externalSpecified)
			return ERROR_RESULT("Declarations with modifiers must be in global scope", lineNumber, currentFilePath);
	}

	return SUCCESS_RESULT;
}

static Result VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;

		PROPAGATE_ERROR(SetModifiers(&varDecl->modifiers, varDecl->lineNumber));

		if (varDecl->modifiers.externalValue)
		{
			if (varDecl->initializer.ptr)
				return ERROR_RESULT("External variables cannot have initializers", varDecl->lineNumber, currentFilePath);
		}
		else
		{
			if (varDecl->externalName)
				return ERROR_RESULT("Only external variables can have external names", varDecl->lineNumber, currentFilePath);
		}

		PROPAGATE_ERROR(ResolveExpression(&varDecl->initializer, true, NULL));
		PROPAGATE_ERROR(ResolveType(&varDecl->type, false, NULL));
		PROPAGATE_ERROR(RegisterDeclaration(varDecl->name, node, varDecl->lineNumber));
		return SUCCESS_RESULT;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;

		PROPAGATE_ERROR(SetModifiers(&funcDecl->modifiers, funcDecl->lineNumber));

		if (funcDecl->modifiers.externalValue)
		{
			if (funcDecl->block.ptr)
				return ERROR_RESULT("External functions cannot have code blocks", funcDecl->lineNumber, currentFilePath);
		}
		else
		{
			if (funcDecl->externalName)
				return ERROR_RESULT("Only external functions can have external names", funcDecl->lineNumber, currentFilePath);
			if (!funcDecl->block.ptr)
				return ERROR_RESULT("Expected code block after function declaration", funcDecl->lineNumber, currentFilePath);
			if (funcDecl->variadic)
				return ERROR_RESULT("Only external functions can be variadic functions", funcDecl->lineNumber, currentFilePath);
		}

		PROPAGATE_ERROR(ResolveType(&funcDecl->type, true, NULL));

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
		StructDeclStmt* structDecl = node->ptr;

		PROPAGATE_ERROR(SetModifiers(&structDecl->modifiers, structDecl->lineNumber));

		if (structDecl->modifiers.externalValue)
			return ERROR_RESULT("Struct declarations can only be internal", structDecl->lineNumber, currentFilePath);

		// if it was generated by an array type skip it
		if (structDecl->isArrayType)
			return SUCCESS_RESULT;

		if (structDecl->members.length == 0)
			return ERROR_RESULT("Cannot define an empty struct", structDecl->lineNumber, currentFilePath);

		PushScope();
		for (size_t i = 0; i < structDecl->members.length; ++i)
			PROPAGATE_ERROR(VisitStatement(structDecl->members.array[i]));
		PopScope(NULL);

		PROPAGATE_ERROR(RegisterDeclaration(structDecl->name, node, structDecl->lineNumber));
		return SUCCESS_RESULT;
	}

	case Node_BlockStatement: return VisitBlock(node->ptr);
	case Node_ExpressionStatement: return ResolveExpression(&((ExpressionStmt*)node->ptr)->expr, true, NULL);
	case Node_Return: return ResolveExpression(&((ReturnStmt*)node->ptr)->expr, true, NULL);

	case Node_Section:
	{
		const SectionStmt* section = node->ptr;
		assert(section->block.type == Node_BlockStatement);
		return VisitBlock(section->block.ptr);
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&ifStmt->expr, true, NULL));
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
		WhileStmt* whileStmt = node->ptr;
		PROPAGATE_ERROR(ResolveExpression(&whileStmt->expr, true, NULL));
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&whileStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_For:
	{
		ForStmt* forStmt = node->ptr;
		PushScope();
		PROPAGATE_ERROR(VisitStatement(&forStmt->initialization));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->condition, true, NULL));
		PROPAGATE_ERROR(ResolveExpression(&forStmt->increment, true, NULL));
		PROPAGATE_ERROR(VisitStatement(&forStmt->stmt));
		PopScope(NULL);
		return SUCCESS_RESULT;
	}
	case Node_Import:
	{
		ImportStmt* import = node->ptr;
		PROPAGATE_ERROR(SetModifiers(&import->modifiers, import->lineNumber));

		if (import->modifiers.externalValue)
			return ERROR_RESULT("Import statements can only be internal", import->lineNumber, currentFilePath);

		PROPAGATE_ERROR(RecursiveRegisterImportNode(node, true));
		return SUCCESS_RESULT;
	}
	case Node_Modifier:
	{
		ModifierStmt* modifier = node->ptr;
		if (modifier->modifierState.publicSpecified)
		{
			currentModifierState.publicSpecified = true;
			currentModifierState.publicValue = modifier->modifierState.publicValue;
		}
		if (modifier->modifierState.externalSpecified)
		{
			currentModifierState.externalSpecified = true;
			currentModifierState.externalValue = modifier->modifierState.externalValue;
		}

		FreeASTNode(*node);
		*node = NULL_NODE;
		return SUCCESS_RESULT;
	}
	case Node_LoopControl:
	case Node_Null:
		return SUCCESS_RESULT;
	default: INVALID_VALUE(node->type);
	}
}

static Result VisitModule(ModuleNode* module)
{
	currentModifierState = (ModifierState){.publicSpecified = false};
	currentFilePath = module->path;
	currentModule = module;

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
	structTypeToArrayStruct = AllocateMap(sizeof(StructDeclStmt*));

	modules = AllocateMap(sizeof(Map));
	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		PROPAGATE_ERROR(VisitModule(node->ptr));
	}

	for (MAP_ITERATE(i, &modules))
		FreeDeclarations(i->value);
	FreeMap(&modules);

	FreeMap(&structTypeToArrayStruct);
	return SUCCESS_RESULT;
}
