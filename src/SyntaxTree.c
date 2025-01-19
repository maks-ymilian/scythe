#include "SyntaxTree.h"

#include <assert.h>
#include <stddef.h>
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
};

UnaryOperator tokenTypeToUnaryOperator[] = {
	[Token_Plus] = Unary_Plus,
	[Token_Minus] = Unary_Minus,
	[Token_Exclamation] = Unary_Negate,
	[Token_PlusPlus] = Unary_Increment,
	[Token_MinusMinus] = Unary_Decrement,
};

TokenType primitiveTypeToTokenType[] = {
	[Primitive_Void] = Token_Void,
	[Primitive_Float] = Token_Float,
	[Primitive_Int] = Token_Int,
	[Primitive_Bool] = Token_Bool,
	[Primitive_String] = Token_String,
};

PrimitiveType tokenTypeToPrimitiveType[] = {
	[Token_Void] = Primitive_Void,
	[Token_Float] = Primitive_Float,
	[Token_Int] = Primitive_Int,
	[Token_Bool] = Primitive_Bool,
	[Token_String] = Primitive_String,
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
		const NodePtr copy = AllocASTNode(ptr, sizeof(BinaryExpr), Node_Binary);
		assert(copy.type == Node_Binary);
		ptr = copy.ptr;

		ptr->left = CopyASTNode(ptr->left);
		ptr->right = CopyASTNode(ptr->right);

		return copy;
	}
	case Node_Unary:
	{
		UnaryExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(UnaryExpr), Node_Unary);
		assert(copy.type == Node_Unary);
		ptr = copy.ptr;

		ptr->expression = CopyASTNode(ptr->expression);

		return copy;
	}
	case Node_Literal:
	{
		LiteralExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(LiteralExpr), Node_Literal);
		assert(copy.type == Node_Literal);
		ptr = copy.ptr;

		if (ptr->type == Literal_Float) ptr->floatValue = AllocateString(ptr->floatValue);
		if (ptr->type == Literal_Identifier) ptr->identifier.text = AllocateString(ptr->identifier.text);
		if (ptr->type == Literal_String) ptr->string = AllocateString(ptr->string);

		return copy;
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(FuncCallExpr), Node_FunctionCall);
		assert(copy.type == Node_FunctionCall);
		ptr = copy.ptr;

		ptr->identifier.text = AllocateString(ptr->identifier.text);

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
	case Node_ArrayAccess:
	{
		ArrayAccessExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(ArrayAccessExpr), Node_ArrayAccess);
		assert(copy.type == Node_ArrayAccess);
		ptr = copy.ptr;

		ptr->subscript = CopyASTNode(ptr->subscript);

		ptr->identifier.text = AllocateString(ptr->identifier.text);

		return copy;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* ptr = node.ptr;
		const NodePtr copy = AllocASTNode(ptr, sizeof(MemberAccessExpr), Node_MemberAccess);
		assert(copy.type == Node_Binary);
		ptr = copy.ptr;

		ptr->next = CopyASTNode(ptr->next);
		ptr->value = CopyASTNode(ptr->value);

		return copy;
	}

	case Node_ExpressionStatement:
	case Node_Import:
	case Node_Section:
	case Node_VariableDeclaration:
	case Node_FunctionDeclaration:
	case Node_StructDeclaration:
	case Node_Block:
	case Node_If:
	case Node_While:
	case Node_For:
	case Node_LoopControl:
	case Node_Return:
	default: unreachable();
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
	case Node_Literal:
	{
		const LiteralExpr* ptr = node.ptr;
		if (ptr->type == Literal_Identifier) free(ptr->identifier.text);
		if (ptr->type == Literal_String) free(ptr->string);
		if (ptr->type == Literal_Float) free(ptr->floatValue);
		break;
	}
	case Node_FunctionCall:
	{
		const FuncCallExpr* ptr = node.ptr;
		free(ptr->identifier.text);
		for (size_t i = 0; i < ptr->arguments.length; ++i)
			FreeASTNode(*(NodePtr*)ptr->arguments.array[i]);
		FreeArray(&ptr->arguments);
		break;
	}
	case Node_ArrayAccess:
	{
		const ArrayAccessExpr* ptr = node.ptr;
		free(ptr->identifier.text);
		FreeASTNode(ptr->subscript);
		break;
	}
	case Node_MemberAccess:
	{
		const MemberAccessExpr* ptr = node.ptr;
		FreeASTNode(ptr->value);
		FreeASTNode(ptr->next);
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
		FreeASTNode(ptr->type);
		FreeASTNode(ptr->initializer);
		FreeASTNode(ptr->arrayLength);
		break;
	}
	case Node_FunctionDeclaration:
	{
		const FuncDeclStmt* ptr = node.ptr;
		free(ptr->name);
		free(ptr->externalName);
		FreeASTNode(ptr->type);
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

	case Node_Block:
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
	case Node_LoopControl: break;

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

	default: unreachable();
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
}
