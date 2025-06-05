#include "SyntaxTree.h"

#include <stdlib.h>
#include <string.h>

#include "StringUtils.h"

TokenType binaryOperatorToTokenType[] = {
	[Binary_BoolAnd] = Token_AmpersandAmpersand,
	[Binary_BoolOr] = Token_PipePipe,
	[Binary_IsEqual] = Token_EqualsEquals,
	[Binary_NotEqual] = Token_ExclamationEquals,
	[Binary_GreaterThan] = Token_RightAngleBracket,
	[Binary_GreaterOrEqual] = Token_RightAngleEquals,
	[Binary_LessThan] = Token_LeftAngleBracket,
	[Binary_LessOrEqual] = Token_LeftAngleEquals,

	[Binary_BitAnd] = Token_Ampersand,
	[Binary_BitOr] = Token_Pipe,
	[Binary_XOR] = Token_Tilde,

	[Binary_Add] = Token_Plus,
	[Binary_Subtract] = Token_Minus,
	[Binary_Multiply] = Token_Asterisk,
	[Binary_Divide] = Token_Slash,
	[Binary_Exponentiation] = Token_Caret,
	[Binary_Modulo] = Token_Percent,
	[Binary_LeftShift] = Token_LeftAngleLeftAngle,
	[Binary_RightShift] = Token_RightAngleRightAngle,

	[Binary_Assignment] = Token_Equals,
	[Binary_AddAssign] = Token_PlusEquals,
	[Binary_SubtractAssign] = Token_MinusEquals,
	[Binary_MultiplyAssign] = Token_AsteriskEquals,
	[Binary_DivideAssign] = Token_SlashEquals,
	[Binary_ModuloAssign] = Token_PercentEquals,
	[Binary_ExponentAssign] = Token_CaretEquals,
	[Binary_BitAndAssign] = Token_AmpersandEquals,
	[Binary_BitOrAssign] = Token_PipeEquals,
	[Binary_XORAssign] = Token_TildeEquals,
};

BinaryOperator tokenTypeToBinaryOperator[] = {
	[Token_AmpersandAmpersand] = Binary_BoolAnd,
	[Token_PipePipe] = Binary_BoolOr,
	[Token_EqualsEquals] = Binary_IsEqual,
	[Token_ExclamationEquals] = Binary_NotEqual,
	[Token_RightAngleBracket] = Binary_GreaterThan,
	[Token_RightAngleEquals] = Binary_GreaterOrEqual,
	[Token_LeftAngleBracket] = Binary_LessThan,
	[Token_LeftAngleEquals] = Binary_LessOrEqual,

	[Token_Ampersand] = Binary_BitAnd,
	[Token_Pipe] = Binary_BitOr,
	[Token_Tilde] = Binary_XOR,

	[Token_Plus] = Binary_Add,
	[Token_Minus] = Binary_Subtract,
	[Token_Asterisk] = Binary_Multiply,
	[Token_Slash] = Binary_Divide,
	[Token_Caret] = Binary_Exponentiation,
	[Token_Percent] = Binary_Modulo,
	[Token_LeftAngleLeftAngle] = Binary_LeftShift,
	[Token_RightAngleRightAngle] = Binary_RightShift,

	[Token_Equals] = Binary_Assignment,
	[Token_PlusEquals] = Binary_AddAssign,
	[Token_MinusEquals] = Binary_SubtractAssign,
	[Token_AsteriskEquals] = Binary_MultiplyAssign,
	[Token_SlashEquals] = Binary_DivideAssign,
	[Token_PercentEquals] = Binary_ModuloAssign,
	[Token_CaretEquals] = Binary_ExponentAssign,
	[Token_AmpersandEquals] = Binary_BitAndAssign,
	[Token_PipeEquals] = Binary_BitOrAssign,
	[Token_TildeEquals] = Binary_XORAssign,
};

TokenType unaryOperatorToTokenType[] = {
	[Unary_Plus] = Token_Plus,
	[Unary_Minus] = Token_Minus,
	[Unary_Negate] = Token_Exclamation,
	[Unary_Increment] = Token_PlusPlus,
	[Unary_Decrement] = Token_MinusMinus,
	[Unary_Dereference] = Token_Asterisk,
};

UnaryOperator tokenTypeToUnaryOperator[] = {
	[Token_Plus] = Unary_Plus,
	[Token_Minus] = Unary_Minus,
	[Token_Exclamation] = Unary_Negate,
	[Token_PlusPlus] = Unary_Increment,
	[Token_MinusMinus] = Unary_Decrement,
	[Token_Asterisk] = Unary_Dereference,
};

TokenType primitiveTypeToTokenType[] = {
	[Primitive_Any] = Token_Any,
	[Primitive_Float] = Token_Float,
	[Primitive_Int] = Token_Int,
	[Primitive_Bool] = Token_Bool,
	[Primitive_Void] = Token_Void,
};

PrimitiveType tokenTypeToPrimitiveType[] = {
	[Token_Any] = Primitive_Any,
	[Token_Float] = Primitive_Float,
	[Token_Int] = Primitive_Int,
	[Token_String] = Primitive_Int,
	[Token_Char] = Primitive_Int,
	[Token_Bool] = Primitive_Bool,
	[Token_Void] = Primitive_Void,
};

BinaryOperator getCompoundAssignmentOperator[] =
	{
		[Binary_AddAssign] = Binary_Add,
		[Binary_SubtractAssign] = Binary_Subtract,
		[Binary_MultiplyAssign] = Binary_Multiply,
		[Binary_DivideAssign] = Binary_Divide,
		[Binary_ModuloAssign] = Binary_Modulo,
		[Binary_ExponentAssign] = Binary_Exponentiation,
		[Binary_BitAndAssign] = Binary_BitAnd,
		[Binary_BitOrAssign] = Binary_BitOr,
		[Binary_XORAssign] = Binary_XOR,
};

NodePtr AllocASTNode(const void* node, const size_t size, const NodeType type)
{
	void* out = malloc(size);
	memcpy(out, node, size);
	return (NodePtr){.ptr = out, .type = type};
}

NodePtr CopyASTNode(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Binary:
	{
		BinaryExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->left = CopyASTNode(ptr->left);
		ptr->right = CopyASTNode(ptr->right);

		return copy;
	}
	case Node_Unary:
	{
		UnaryExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->expression = CopyASTNode(ptr->expression);

		return copy;
	}
	case Node_Literal:
	{
		LiteralExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		if (ptr->type == Literal_Float) ptr->floatValue = AllocateString(ptr->floatValue);
		if (ptr->type == Literal_String) ptr->string = AllocateString(ptr->string);

		return copy;
	}
	case Node_Subscript:
	{
		SubscriptExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == Node_Subscript);
		ptr = copy.ptr;

		ptr->baseExpr = CopyASTNode(ptr->baseExpr);
		ptr->indexExpr = CopyASTNode(ptr->indexExpr);
		if (ptr->typeBeforeCollapse.expr.ptr)
			ptr->typeBeforeCollapse.expr = CopyASTNode(ptr->typeBeforeCollapse.expr);

		return copy;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->baseExpr = CopyASTNode(ptr->baseExpr);

		Array arguments = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->arguments.length; ++i)
		{
			const NodePtr* node = ptr->arguments.array[i];
			const NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&arguments, &copy);
		}
		ptr->arguments = arguments;

		return copy;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		if (ptr->start.ptr != NULL) ptr->start = CopyASTNode(ptr->start);

		Array identifiers = AllocateArray(sizeof(char*));
		for (size_t i = 0; i < ptr->identifiers.length; ++i)
		{
			char* str = *(char**)ptr->identifiers.array[i];
			char* copy = AllocateString(str);
			ArrayAdd(&identifiers, &copy);
		}
		ptr->identifiers = identifiers;

		return copy;
	}
	case Node_BlockExpression:
	{
		BlockExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->block = CopyASTNode(ptr->block);
		ptr->type.expr = CopyASTNode(ptr->type.expr);

		return copy;
	}
	case Node_SizeOf:
	{
		SizeOfExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		if (ptr->expr.ptr) ptr->expr = CopyASTNode(ptr->expr);
		if (ptr->type.expr.ptr) ptr->type.expr = CopyASTNode(ptr->type.expr);

		return copy;
	}

	case Node_ExpressionStatement:
	{
		ExpressionStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->expr = CopyASTNode(ptr->expr);

		return copy;
	}
	case Node_Import:
	{
		ImportStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->path = AllocateString(ptr->path);
		ptr->moduleName = AllocateString(ptr->moduleName);

		return copy;
	}
	case Node_Section:
	{
		SectionStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->block = CopyASTNode(ptr->block);

		return copy;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		if (ptr->initializer.ptr) ptr->initializer = CopyASTNode(ptr->initializer);
		ptr->type.expr = CopyASTNode(ptr->type.expr);

		ptr->name = AllocateString(ptr->name);
		ptr->externalName = AllocateString(ptr->externalName);

		Array instantiated = AllocateArray(sizeof(VarDeclStmt*));
		for (size_t i = 0; i < ptr->instantiatedVariables.length; ++i)
		{
			const VarDeclStmt** var = ptr->instantiatedVariables.array[i];
			ArrayAdd(&instantiated, var);
		}
		ptr->instantiatedVariables = instantiated;

		return copy;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->type.expr = CopyASTNode(ptr->type.expr);
		ptr->oldType.expr = CopyASTNode(ptr->oldType.expr);

		ptr->name = AllocateString(ptr->name);
		ptr->externalName = AllocateString(ptr->externalName);

		Array parameters = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->parameters.length; ++i)
		{
			NodePtr* node = ptr->parameters.array[0];
			NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&parameters, &copy);
		}
		ptr->parameters = parameters;

		Array oldParameters = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->oldParameters.length; ++i)
		{
			NodePtr* node = ptr->oldParameters.array[0];
			NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&oldParameters, &copy);
		}
		ptr->oldParameters = oldParameters;

		ptr->block = CopyASTNode(ptr->block);

		return copy;
	}
	case Node_StructDeclaration:
	{
		StructDeclStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->name = AllocateString(ptr->name);

		Array members = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->members.length; ++i)
		{
			NodePtr* node = ptr->members.array[i];
			NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&members, &copy);
		}
		ptr->members = members;

		return copy;
	}
	case Node_Modifier:
	{
		ModifierStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		return copy;
	}
	case Node_BlockStatement:
	{
		BlockStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		Array statements = AllocateArray(sizeof(NodePtr));
		for (size_t i = 0; i < ptr->statements.length; ++i)
		{
			NodePtr* node = ptr->statements.array[i];
			NodePtr copy = CopyASTNode(*node);
			ArrayAdd(&statements, &copy);
		}
		ptr->statements = statements;

		return copy;
	}
	case Node_If:
	{
		IfStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->expr = CopyASTNode(ptr->expr);
		ptr->trueStmt = CopyASTNode(ptr->trueStmt);
		ptr->falseStmt = CopyASTNode(ptr->falseStmt);

		return copy;
	}
	case Node_While:
	{
		WhileStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->expr = CopyASTNode(ptr->expr);
		ptr->stmt = CopyASTNode(ptr->stmt);

		return copy;
	}
	case Node_For:
	{
		ForStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->initialization = CopyASTNode(ptr->initialization);
		ptr->condition = CopyASTNode(ptr->condition);
		ptr->increment = CopyASTNode(ptr->increment);
		ptr->stmt = CopyASTNode(ptr->stmt);

		return copy;
	}
	case Node_LoopControl:
	{
		LoopControlStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		return copy;
	}
	case Node_Return:
	{
		ReturnStmt* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(*ptr), node.type);
		ASSERT(copy.type == node.type);
		ptr = copy.ptr;

		ptr->expr = CopyASTNode(ptr->expr);

		return copy;
	}
	default: INVALID_VALUE(node.type);
	}
}

void FreeASTNode(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Null: return;

	case Node_Binary:
	{
		const BinaryExpr* ptr = node.ptr;
		FreeASTNode(ptr->left);
		FreeASTNode(ptr->right);
		break;
	}
	case Node_Unary:
	{
		const UnaryExpr* ptr = node.ptr;
		FreeASTNode(ptr->expression);
		break;
	}
	case Node_Subscript:
	{
		const SubscriptExpr* ptr = node.ptr;
		FreeASTNode(ptr->baseExpr);
		FreeASTNode(ptr->indexExpr);
		FreeASTNode(ptr->typeBeforeCollapse.expr);
		break;
	}
	case Node_Literal:
	{
		const LiteralExpr* ptr = node.ptr;
		if (ptr->type == Literal_String) free(ptr->string);
		if (ptr->type == Literal_Float) free(ptr->floatValue);
		break;
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* ptr = node.ptr;
		FreeASTNode(ptr->baseExpr);
		for (size_t i = 0; i < ptr->arguments.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->arguments.array[i]);
		FreeArray(&ptr->arguments);
		break;
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* ptr = node.ptr;
		FreeASTNode(ptr->start);

		for (size_t i = 0; i < ptr->identifiers.length; ++i)
			free(*(char**)ptr->identifiers.array[i]);
		if (ptr->identifiers.array != NULL) FreeArray(&ptr->identifiers);
		break;
	}
	case Node_BlockExpression:
	{
		const BlockExpr* ptr = node.ptr;
		FreeASTNode(ptr->block);
		FreeASTNode(ptr->type.expr);
		break;
	}
	case Node_SizeOf:
	{
		const SizeOfExpr* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		FreeASTNode(ptr->type.expr);
		break;
	}

	case Node_ExpressionStatement:
	{
		const ExpressionStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		break;
	}

	case Node_Import:
	{
		const ImportStmt* ptr = node.ptr;
		free(ptr->path);
		free(ptr->moduleName);
		break;
	}
	case Node_Section:
	{
		const SectionStmt* ptr = node.ptr;
		FreeASTNode(ptr->block);
		break;
	}
	case Node_VariableDeclaration:
	{
		const VarDeclStmt* ptr = node.ptr;
		free(ptr->name);
		free(ptr->externalName);
		FreeASTNode(ptr->type.expr);
		FreeASTNode(ptr->initializer);
		FreeArray(&ptr->instantiatedVariables);
		break;
	}
	case Node_FunctionDeclaration:
	{
		const FuncDeclStmt* ptr = node.ptr;
		free(ptr->name);
		free(ptr->externalName);
		FreeASTNode(ptr->type.expr);
		for (size_t i = 0; i < ptr->parameters.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->parameters.array[i]);
		FreeArray(&ptr->parameters);
		break;
	}
	case Node_StructDeclaration:
	{
		const StructDeclStmt* ptr = node.ptr;
		free(ptr->name);
		for (size_t i = 0; i < ptr->members.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->members.array[i]);
		FreeArray(&ptr->members);
		break;
	}
	case Node_Modifier:
		break;

	case Node_BlockStatement:
	{
		const BlockStmt* ptr = node.ptr;
		for (size_t i = 0; i < ptr->statements.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->statements.array[i]);
		FreeArray(&ptr->statements);
		break;
	}

	case Node_If:
	{
		const IfStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		FreeASTNode(ptr->trueStmt);
		FreeASTNode(ptr->falseStmt);
		break;
	}
	case Node_While:
	{
		const WhileStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		FreeASTNode(ptr->stmt);
		break;
	}
	case Node_For:
	{
		const ForStmt* ptr = node.ptr;
		FreeASTNode(ptr->condition);
		FreeASTNode(ptr->initialization);
		FreeASTNode(ptr->increment);
		FreeASTNode(ptr->stmt);
		break;
	}
	case Node_LoopControl:
		break;

	case Node_Return:
	{
		const ReturnStmt* ptr = node.ptr;
		FreeASTNode(ptr->expr);
		break;
	}

	case Node_Module:
	{
		const ModuleNode* ptr = node.ptr;
		for (size_t i = 0; i < ptr->statements.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->statements.array[i]);
		free(ptr->path);
		free(ptr->moduleName);
		break;
	}

	default: INVALID_VALUE(node.type);
	}

	free(node.ptr);
}

void FreeAST(const AST root)
{
	for (size_t i = 0; i < root.nodes.length; ++i)
	{
		const NodePtr* node = root.nodes.array[i];
		FreeASTNode(*node);
	}

	FreeArray(&root.nodes);
}
