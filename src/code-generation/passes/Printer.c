#include "Printer.h"

#include <string.h>
#include <stdarg.h>

static int indentationLevel;

static void PrintIndentation(void)
{
	printf("\r");
	for (int i = 0; i < indentationLevel; ++i)
		printf("|\t");
}

static void Print(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	vprintf(format, args);

	if (format[strlen(format) - 1] == '\n')
		PrintIndentation();

	va_end(args);
}

static void PushIndent(void)
{
	++indentationLevel;
	PrintIndentation();
}

static void PopIndent(void)
{
	--indentationLevel;
	PrintIndentation();
}

static void VisitStatement(const NodePtr* node);
static void VisitExpression(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_Binary:
	{
		Print("Binary\n");
		PushIndent();


		const BinaryExpr* binary = node->ptr;
		Print("operatorType: %d\n", binary->operatorType);

		Print("left: ");
		VisitExpression(&binary->left);
		Print("right: ");
		VisitExpression(&binary->right);

		PopIndent();
		break;
	}
	case Node_Unary:
	{
		Print("Unary\n");

		PushIndent();
		const UnaryExpr* unary = node->ptr;
		VisitExpression(&unary->expression);

		PopIndent();
		break;
	}
	case Node_MemberAccess:
	{
		Print("MemberAccess\n");
		PushIndent();

		const MemberAccessExpr* memberAccess = node->ptr;

		Print("identifiers: ");
		for (size_t i = 0; i < memberAccess->identifiers.length; ++i)
			Print("%s ", *(char**)memberAccess->identifiers.array[i]);
		Print("\n");

		Print("start:\n");
		PushIndent();
		VisitExpression(&memberAccess->start);
		PopIndent();

		PopIndent();
		break;
	}
	case Node_Subscript:
	{
		Print("Subscript\n");
		PushIndent();

		const SubscriptExpr* subscript = node->ptr;
		VisitExpression(&subscript->baseExpr);
		VisitExpression(&subscript->indexExpr);

		PopIndent();
		break;
	}
	case Node_FunctionCall:
	{
		Print("FunctionCall\n");
		PushIndent();

		const FuncCallExpr* funcCall = node->ptr;
		Print("base: ");
		VisitExpression(&funcCall->baseExpr);

		Print("arguments:\n");
		PushIndent();
		for (size_t i = 0; i < funcCall->arguments.length; ++i)
			VisitExpression(funcCall->arguments.array[i]);
		PopIndent();

		PopIndent();
		break;
	}
	case Node_BlockExpression:
	{
		Print("BlockExpression\n");
		PushIndent();

		// const BlockExpr* blockExpr = node->ptr;
		// VisitStatement(&blockExpr->block);

		PopIndent();
		break;
	}
	case Node_Literal:
	{
		Print("Literal\n");
		PushIndent();

		LiteralExpr* literal = node->ptr;
		if (literal->type == Literal_Number)
			Print("%s\n", literal->number);

		PopIndent();
		break;
	}
	case Node_SizeOf:
	{
		Print("SizeOf\n");
		break;
	}
	case Node_Null:
	{
		Print("Null\n");
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

static void VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_BlockStatement:
	{
		Print("BlockStatement\n");
		PushIndent();

		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);

		PopIndent();
		break;
	}
	case Node_Section:
	{
		Print("Section\n");
		PushIndent();

		const SectionStmt* section = node->ptr;
		VisitStatement(&section->block);

		PopIndent();
		break;
	}
	case Node_If:
	{
		Print("If\n");
		PushIndent();

		const IfStmt* ifStmt = node->ptr;
		VisitStatement(&ifStmt->trueStmt);
		VisitStatement(&ifStmt->falseStmt);
		VisitExpression(&ifStmt->expr);

		PopIndent();
		break;
	}
	case Node_FunctionDeclaration:
	{
		Print("FuncDeclStmt\n");
		PushIndent();

		const FuncDeclStmt* funcDecl = node->ptr;
		for (size_t i = 0; i < funcDecl->parameters.length; ++i)
			VisitStatement(funcDecl->parameters.array[i]);
		VisitStatement(&funcDecl->block);

		PopIndent();
		break;
	}
	case Node_Return:
	{
		Print("ReturnStmt\n");
		PushIndent();

		const ReturnStmt* returnStmt = node->ptr;
		VisitExpression(&returnStmt->expr);

		PopIndent();
		break;
	}
	case Node_VariableDeclaration:
	{
		Print("VarDeclStmt\n");
		PushIndent();

		const VarDeclStmt* varDecl = node->ptr;
		VisitExpression(&varDecl->initializer);

		PopIndent();
		break;
	}
	case Node_StructDeclaration:
	{
		Print("StructDeclStmt\n");
		PushIndent();

		const StructDeclStmt* structDecl = node->ptr;
		for (size_t i = 0; i < structDecl->members.length; ++i)
			VisitStatement(structDecl->members.array[i]);

		PopIndent();
		break;
	}
	case Node_ExpressionStatement:
	{
		Print("ExpressionStmt\n");
		PushIndent();

		const ExpressionStmt* exprStmt = node->ptr;
		VisitExpression(&exprStmt->expr);

		PopIndent();
		break;
	}
	case Node_While:
	{
		Print("WhileStmt\n");
		PushIndent();

		const WhileStmt* whileStmt = node->ptr;
		VisitExpression(&whileStmt->expr);
		VisitStatement(&whileStmt->stmt);

		PopIndent();
		break;
	}
	case Node_For:
	{
		Print("ForStmt\n");
		PushIndent();

		const ForStmt* forStmt = node->ptr;
		VisitStatement(&forStmt->initialization);
		VisitExpression(&forStmt->condition);
		VisitExpression(&forStmt->increment);
		VisitStatement(&forStmt->stmt);

		PopIndent();
		break;
	}
	case Node_Import:
	{
		Print("ImportStmt\n");
		PushIndent();

		const ImportStmt* importStmt = node->ptr;
		Print("path:%s\n", importStmt->path);
		Print("module:%s\n", importStmt->moduleName);

		PopIndent();
		break;
	}
	case Node_Input:
	{
		Print("InputStmt\n");
		break;
	}
	case Node_Modifier:
	{
		Print("ModifierStmt\n");
		break;
	}
	case Node_LoopControl:
	{
		Print("LoopControlStmt\n");
		break;
	}
	case Node_Null:
	{
		Print("Null\n");
		break;
	}
	default: INVALID_VALUE(node->type);
	}
}

void Printer(const AST* ast)
{
	indentationLevel = 0;

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];

		ASSERT(node->type == Node_Module);
		const ModuleNode* module = node->ptr;

		Print("*************************************** MODULE: %s *****************************************\n", module->moduleName);
		ASSERT(indentationLevel == 0);
		PushIndent();

		for (size_t i = 0; i < module->statements.length; ++i)
			VisitStatement(module->statements.array[i]);

		PopIndent();
	}
}
