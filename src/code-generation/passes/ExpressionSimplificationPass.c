#include "ExpressionSimplificationPass.h"

#include "Common.h"

#include <ctype.h>
#include <string.h>

typedef struct
{
	union
	{
		uint64_t intValue;
		double floatValue;
	};
	bool isInt;
	bool isNegative;
} ConstantInfo;

static void VisitStatement(NodePtr* node);

static uint64_t PowerOf10(int x)
{
	switch (x)
	{
	case 0: return 1ULL;
	case 1: return 10ULL;
	case 2: return 100ULL;
	case 3: return 1000ULL;
	case 4: return 10000ULL;
	case 5: return 100000ULL;
	case 6: return 1000000ULL;
	case 7: return 10000000ULL;
	case 8: return 100000000ULL;
	case 9: return 1000000000ULL;
	case 10: return 10000000000ULL;
	case 11: return 100000000000ULL;
	case 12: return 1000000000000ULL;
	case 13: return 10000000000000ULL;
	case 14: return 100000000000000ULL;
	case 15: return 1000000000000000ULL;
	case 16: return 10000000000000000ULL;
	case 17: return 100000000000000000ULL;
	case 18: return 1000000000000000000ULL;
	case 19: return 10000000000000000000ULL;
	default: INVALID_VALUE(x);
	}
}

static NodePtr AllocNumberExpr(ConstantInfo info, int lineNumber)
{
	ASSERT(info.isInt);

	if (info.isNegative)
	{
		info.isNegative = false;
		return AllocASTNode(
			&(UnaryExpr){
				.lineNumber = lineNumber,
				.operatorType = Unary_Minus,
				.expression = AllocNumberExpr(info, lineNumber),
			},
			sizeof(UnaryExpr), Node_Unary);
	}
	else
		return AllocUInt64Integer(info.intValue, lineNumber);
}

static bool VisitExpression(NodePtr* node, ConstantInfo* info)
{
	ConstantInfo a;
	if (!info)
		info = &a;

	switch (node->type)
	{
	case Node_Null:
		return false;
	case Node_Literal:
	{
		LiteralExpr* literal = node->ptr;
		switch (literal->type)
		{
		case Literal_String:
		case Literal_Char:
			return false;
		case Literal_Number:
		{
			size_t length = strlen(literal->number);
			ASSERT(length > 0);

			for (size_t i = 0; i < length; ++i)
				if (literal->number[i] == '.')
					return false;

			if (literal->number[0] == '-') // todo
				return false;

			ASSERT(length <= 20);

			uint64_t num = 0;
			for (size_t i = 0; i < length; ++i)
			{
				ASSERT(isdigit(literal->number[i]));
				int digit = literal->number[i] - '0';
				num += (uint64_t)digit * PowerOf10((int)(length - 1) - (int)i);
			}
			*info = (ConstantInfo){.isInt = true, .intValue = num};
			return true;
		}
		default: INVALID_VALUE(literal->type);
		}
	}
	case Node_FunctionCall:
	{
		FuncCallExpr* funcCall = node->ptr;
		VisitExpression(&funcCall->baseExpr, NULL);
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(funcCall->arguments.array[i], NULL);
		return false;
	}
	case Node_BlockExpression:
	{
		BlockExpr* blockExpr = node->ptr;
		VisitStatement(&blockExpr->block);
		return false;
	}
	case Node_MemberAccess:
	{
		MemberAccessExpr* memberAccess = node->ptr;
		VisitExpression(&memberAccess->start, NULL);
		return false;
	}
	case Node_Binary:
	{
		BinaryExpr* binary = node->ptr;
		ConstantInfo leftInfo;
		ConstantInfo rightInfo;
		bool left = VisitExpression(&binary->left, &leftInfo);
		bool right = VisitExpression(&binary->right, &rightInfo);
		switch (binary->operatorType)
		{
		case Binary_BoolAnd:
		{
			if (left && right && leftInfo.isInt && rightInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = leftInfo.intValue && rightInfo.intValue};
				*node = AllocNumberExpr(*info, binary->lineNumber);
				return true;
			}
			else if (left && !right && leftInfo.isInt && leftInfo.intValue)
			{
				*node = binary->right;
				return false;
			}
			else if (!left && right && rightInfo.isInt && rightInfo.intValue)
			{
				*node = binary->left;
				return false;
			}
			else
				return false;
		}
		case Binary_BoolOr:
		{
			return false;
		}
		case Binary_IsEqual:
		{
			if (left && right && leftInfo.isInt && rightInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = leftInfo.intValue == rightInfo.intValue && leftInfo.isNegative == rightInfo.isNegative};
				*node = AllocNumberExpr(*info, binary->lineNumber);
				return true;
			}
			else
				return false;
		}
		case Binary_NotEqual:
		{
			if (left && right && leftInfo.isInt && rightInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = leftInfo.intValue != rightInfo.intValue || leftInfo.isNegative != rightInfo.isNegative};
				*node = AllocNumberExpr(*info, binary->lineNumber);
				return true;
			}
			else
				return false;
		}
		case Binary_GreaterThan:
		{
			return false;
		}
		case Binary_GreaterOrEqual:
		{
			return false;
		}
		case Binary_LessThan:
		{
			return false;
		}
		case Binary_LessOrEqual:
		{
			return false;
		}

		case Binary_BitAnd:
		{
			return false;
		}
		case Binary_BitOr:
		{
			if (left && right && leftInfo.isInt && rightInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = leftInfo.intValue | rightInfo.intValue, .isNegative = leftInfo.isNegative | rightInfo.isNegative};
				*node = AllocNumberExpr(*info, binary->lineNumber);
				return true;
			}
			else
				return false;
		}
		case Binary_XOR:
		{
			return false;
		}

		case Binary_Add:
		{
			return false;
		}
		case Binary_Subtract:
		{
			return false;
		}
		case Binary_Multiply:
		{
			return false;
		}
		case Binary_Divide:
		{
			return false;
		}
		case Binary_Exponentiation:
		{
			return false;
		}
		case Binary_Modulo:
		{
			return false;
		}
		case Binary_LeftShift:
		{
			return false;
		}
		case Binary_RightShift:
		{
			return false;
		}

		case Binary_Assignment:
		{
			return false;
		}
		default: INVALID_VALUE(binary->operatorType);
		}
	}
	case Node_Unary:
	{
		UnaryExpr* unary = node->ptr;
		ConstantInfo unaryInfo;
		if (!VisitExpression(&unary->expression, &unaryInfo))
			return false;

		switch (unary->operatorType)
		{
		case Unary_Negate:
		{
			if (unaryInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = !unaryInfo.intValue};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			else
			{
				*info = (ConstantInfo){.isInt = true, .intValue = !(bool)unaryInfo.floatValue};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			return true;
		}
		case Unary_Minus:
		{
			if (unaryInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = unaryInfo.intValue, .isNegative = !unaryInfo.isNegative};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			else
			{
				*info = (ConstantInfo){.isInt = false, .floatValue = unaryInfo.floatValue, .isNegative = !unaryInfo.isNegative};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			return true;
		}
		case Unary_Plus:
		{
			if (unaryInfo.isInt)
			{
				*info = (ConstantInfo){.isInt = true, .intValue = unaryInfo.intValue, .isNegative = unaryInfo.isNegative};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			else
			{
				*info = (ConstantInfo){.isInt = false, .floatValue = unaryInfo.floatValue, .isNegative = unaryInfo.isNegative};
				*node = AllocNumberExpr(*info, unary->lineNumber);
			}
			return true;
		}
		default: INVALID_VALUE(unary->operatorType);
		}
	}
	case Node_Subscript:
	{
		SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr, NULL);
		VisitExpression(&subscript->indexExpr, NULL);
		return false;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(NodePtr* node)
{
	switch (node->type)
	{
	case Node_Import:
	case Node_StructDeclaration:
	case Node_Desc:
	case Node_Null:
		break;
	case Node_ExpressionStatement:
	{
		ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr, NULL);
		break;
	}
	case Node_VariableDeclaration:
	{
		VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer, NULL);
		break;
	}
	case Node_FunctionDeclaration:
	{
		FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);
		break;
	}
	case Node_Input:
	{
		InputStmt* input = node->ptr;
		ASSERT(input->varDecl.type == Node_VariableDeclaration);
		VisitStatement(&input->varDecl);
		break;
	}
	case Node_BlockStatement:
	{
		BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	}
	case Node_If:
	{
		IfStmt* ifStmt = node->ptr;
		VisitExpression(&ifStmt->expr, NULL);
		VisitStatement(&ifStmt->falseStmt);
		VisitStatement(&ifStmt->trueStmt);
		break;
	}
	case Node_Section:
	{
		SectionStmt* section = node->ptr;
		ASSERT(section->block.type == Node_BlockStatement);
		VisitStatement(&section->block);
		break;
	}
	case Node_While:
	{
		WhileStmt* whileStmt = node->ptr;
		VisitExpression(&whileStmt->expr, NULL);
		VisitStatement(&whileStmt->stmt);
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void ExpressionSimplificationPass(const AST* ast)
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
