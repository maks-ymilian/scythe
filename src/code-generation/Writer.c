#include "Writer.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../data-structures/MemoryStream.h"

#define INDENT_WIDTH 4
#define INDENT_STRING "    "

static MemoryStream* stream;

static int indentationLevel;

static void WriteInteger(const long integer)
{
	const int size = snprintf(NULL, 0, "%ld", integer);
	assert(size > 0);
	char string[size + 1];
	snprintf(string, sizeof(string), "%ld", integer);
	StreamWrite(stream, string, sizeof(string) - 1);
}

static void WriteFloat(const double value)
{
	const int size = snprintf(NULL, 0, "%.14lf\n", value);
	assert(size > 0);
	char string[size + 1];
	snprintf(NULL, 0, "%.14lf\n", value);
	StreamWrite(stream, string, sizeof(string) - 1);
}

static void WriteString(const char* str)
{
	const size_t length = strlen(str);
	if (length == 0)
		return;

	StreamWrite(stream, str, length);

	if (str[length - 1] == '\n')
	{
		for (int i = 0; i < indentationLevel; ++i)
			StreamWrite(stream, INDENT_STRING, INDENT_WIDTH);
	}
}

static void WriteChar(const char chr)
{
	StreamWriteByte(stream, (uint8_t)chr);

	if (chr == '\n')
	{
		for (int i = 0; i < indentationLevel; ++i)
			StreamWrite(stream, INDENT_STRING, INDENT_WIDTH);
	}
}

static void PushIndent()
{
	StreamWrite(stream, INDENT_STRING, INDENT_WIDTH);
	indentationLevel++;
}

static void PopIndent()
{
	const Buffer lastChars = StreamRewindRead(stream, INDENT_WIDTH);
	if (memcmp(INDENT_STRING, lastChars.buffer, INDENT_WIDTH) == 0)
		StreamRewind(stream, INDENT_WIDTH);
	indentationLevel--;
}

static void VisitExpression(NodePtr node);

static void VisitFunctionCall(const FuncCallExpr* funcCall)
{
	WriteString(funcCall->identifier.text);
	WriteChar('(');
	for (size_t i = 0; i < funcCall->parameters.length; ++i)
	{
		VisitExpression(*(NodePtr*)funcCall->parameters.array[i]);
		if (i < funcCall->parameters.length - 1)
			WriteString(", ");
	}
	WriteChar(')');
}

static void VisitLiteralExpression(const LiteralExpr* literal)
{
	switch (literal->type)
	{
	case Literal_Float: WriteString(literal->floatValue); break;
	case Literal_Int: WriteInteger(literal->intValue); break;
	case Literal_Identifier: WriteString(literal->identifier.text); break;
	case Literal_String:
		WriteChar('\"');
		WriteString(literal->string);
		WriteChar('\"');
		break;
	default: unreachable();
	}
}

static void VisitBinaryExpression(const BinaryExpr* binary)
{
	char* operator;
	switch (binary->operatorType)
	{
	case Binary_BoolAnd: operator= "&&"; break;
	case Binary_BoolOr: operator= "||"; break;
	case Binary_IsEqual: operator= "=="; break;
	case Binary_NotEqual: operator= "!="; break;
	case Binary_GreaterThan: operator= ">"; break;
	case Binary_GreaterOrEqual: operator= ">="; break;
	case Binary_LessThan: operator= "<"; break;
	case Binary_LessOrEqual: operator= "<="; break;

	case Binary_BitAnd: operator= "&"; break;
	case Binary_BitOr: operator= "|"; break;
	case Binary_XOR: operator= "~"; break;

	case Binary_Add: operator= "+"; break;
	case Binary_Subtract: operator= "-"; break;
	case Binary_Multiply: operator= "*"; break;
	case Binary_Divide: operator= "/"; break;
	case Binary_Exponentiation: operator= "^"; break;
	case Binary_Modulo: operator= "%"; break;
	case Binary_LeftShift: operator= "<<"; break;
	case Binary_RightShift: operator= ">>"; break;

	case Binary_Assignment: operator= "="; break;
	case Binary_AddAssign: operator= "+="; break;
	case Binary_SubtractAssign: operator= "-="; break;
	case Binary_MultiplyAssign: operator= "*="; break;
	case Binary_DivideAssign: operator= "/="; break;
	case Binary_ModuloAssign: operator= "%="; break;
	case Binary_ExponentAssign: operator= "^="; break;
	case Binary_BitAndAssign: operator= "&="; break;
	case Binary_BitOrAssign: operator= "|="; break;
	case Binary_XORAssign: operator= "~="; break;

	default: unreachable();
	}

	WriteChar('(');
	VisitExpression(binary->left);
	WriteChar(' ');
	WriteString(operator);
	WriteChar(' ');
	VisitExpression(binary->right);
	WriteChar(')');
}

static void VisitExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Binary: VisitBinaryExpression(node.ptr); break;
	case Node_Literal: VisitLiteralExpression(node.ptr); break;
	case Node_FunctionCall: VisitFunctionCall(node.ptr); break;
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = node.ptr;
		assert(memberAccess->next.ptr == NULL);
		if (memberAccess->value.type == Node_Literal)
			VisitLiteralExpression(memberAccess->value.ptr);
		else if (memberAccess->value.type == Node_FunctionCall)
			VisitFunctionCall(memberAccess->value.ptr);
		else
			unreachable();
		break;
	default: unreachable();
	}
}

static void VisitVariableDeclaration(const VarDeclStmt* varDecl)
{
	assert(varDecl->initializer.ptr != NULL);

	VisitBinaryExpression(&(BinaryExpr){
		.lineNumber = -1,
		.operatorType = Binary_Assignment,
		.right = varDecl->initializer,
		.left = (NodePtr){
			&(LiteralExpr){
				.lineNumber = -1,
				.type = Literal_Identifier,
				.identifier = (IdentifierReference){.text = varDecl->name, .reference = NULL_NODE},
			},
			Node_Literal},
	});
	WriteString(";\n");
}

static void VisitStatement(const NodePtr* node);

static void VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
	WriteString("function ");
	WriteString(funcDecl->name);

	WriteChar('(');
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];

		assert(node->type == Node_VariableDeclaration);
		const VarDeclStmt* varDecl = node->ptr;
		WriteString(varDecl->name);

		if (i < funcDecl->parameters.length - 1)
			WriteString(", ");
	}
	WriteString(")\n");

	WriteString("(\n");

	PushIndent();
	VisitStatement(&funcDecl->block);
	PopIndent();

	WriteString(");\n");
}

static void VisitIfStatement(const IfStmt* ifStmt)
{
	VisitExpression(ifStmt->expr);
	WriteString(" ?\n");
	WriteString("(\n");

	PushIndent();
	VisitStatement(&ifStmt->trueStmt);
	PopIndent();

	WriteChar(')');

	if (ifStmt->falseStmt.ptr != NULL)
	{
		WriteString(" : (\n");

		PushIndent();
		VisitStatement(&ifStmt->falseStmt);
		PopIndent();

		WriteChar(')');
	}

	WriteString(";\n");
}

static void VisitStatement(const NodePtr* node)
{
	switch (node->type)
	{
	case Node_FunctionDeclaration:
		VisitFunctionDeclaration(node->ptr);
		break;
	case Node_VariableDeclaration:
		VisitVariableDeclaration(node->ptr);
		break;
	case Node_ExpressionStatement:
		const ExpressionStmt* expressionStmt = node->ptr;
		VisitExpression(expressionStmt->expr);
		WriteString(";\n");
		break;
	case Node_Block:
		const BlockStmt* block = node->ptr;
		for (size_t i = 0; i < block->statements.length; ++i)
			VisitStatement(block->statements.array[i]);
		break;
	case Node_If:
		VisitIfStatement(node->ptr);
		break;

		// temporary
	case Node_StructDeclaration:
		WriteString("struct\n");
		break;
	default: unreachable();
	}
}

static void VisitSection(const SectionStmt* section)
{
	const char* sectionText = NULL;
	switch (section->sectionType)
	{
	case Section_Init: sectionText = "init"; break;
	case Section_Block: sectionText = "block"; break;
	case Section_Sample: sectionText = "sample"; break;
	case Section_Serialize: sectionText = "serialize"; break;
	case Section_Slider: sectionText = "slider"; break;
	case Section_GFX: sectionText = "gfx"; break;
	default: unreachable();
	}

	WriteChar('@');
	WriteString(sectionText);
	WriteChar('\n');

	assert(section->block.type == Node_Block);
	const BlockStmt* block = section->block.ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
		VisitStatement(block->statements.array[i]);

	WriteChar('\n');
}

static void VisitModule(const ModuleNode* module)
{
	if (module->statements.length != 0)
	{
		WriteString("// Module: ");
		WriteString(module->path);
		WriteChar('\n');
	}

	for (size_t i = 0; i < module->statements.length; ++i)
	{
		const NodePtr* stmt = module->statements.array[i];
		switch (stmt->type)
		{
		case Node_Section:
			VisitSection(stmt->ptr);
			break;
		case Node_Import:
			break;
		default: unreachable();
		}
	}
}

void WriteOutput(const AST* ast, char** outBuffer, size_t* outLength)
{
	indentationLevel = 0;
	stream = AllocateMemoryStream();

	for (size_t i = 0; i < ast->nodes.length; ++i)
	{
		const NodePtr* node = ast->nodes.array[i];
		assert(node->type == Node_Module);
		VisitModule(node->ptr);
	}

	const Buffer buffer = StreamGetBuffer(stream);
	FreeMemoryStream(stream, false);
	if (outBuffer != NULL) *outBuffer = (char*)buffer.buffer;
	if (outLength != NULL) *outLength = buffer.length;
}
