#include "Writer.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "data-structures/MemoryStream.h"

#define INDENT_WIDTH 4
#define INDENT_STRING "    "

static MemoryStream* stream;

static int indentationLevel;

static const int binaryPrecedence[] = {
	[Binary_Exponentiation] = 18,
	[Binary_Modulo] = 17,
	[Binary_LeftShift] = 16,
	[Binary_RightShift] = 16,
	[Binary_Multiply] = 15,
	[Binary_Divide] = 15,
	[Binary_Add] = 14,
	[Binary_Subtract] = 14,

	[Binary_BitOr] = 13,
	[Binary_BitAnd] = 12,
	[Binary_XOR] = 11,

	[Binary_IsEqual] = 10,
	[Binary_NotEqual] = 10,
	[Binary_LessThan] = 10,
	[Binary_GreaterThan] = 10,
	[Binary_LessOrEqual] = 10,
	[Binary_GreaterOrEqual] = 10,

	[Binary_BoolOr] = 9,
	[Binary_BoolAnd] = 8,

	[Binary_Assignment] = 7,
	[Binary_MultiplyAssign] = 6,
	[Binary_DivideAssign] = 6,
	[Binary_ModuloAssign] = 5,
	[Binary_ExponentAssign] = 4,
	[Binary_AddAssign] = 3,
	[Binary_SubtractAssign] = 3,
	[Binary_BitOrAssign] = 2,
	[Binary_BitAndAssign] = 1,
	[Binary_XORAssign] = 0,
};

static void WriteUInt64(const uint64_t integer)
{
	const int size = snprintf(NULL, 0, "%" PRIu64, integer);
	assert(size > 0);
	char string[size + 1];
	snprintf(string, sizeof(string), "%" PRIu64, integer);
	StreamWrite(stream, string, sizeof(string) - 1);
}

static void WriteUniqueName(const int uniqueName)
{
	assert(uniqueName > 0);

	const int size = snprintf(NULL, 0, "%d", uniqueName);
	assert(size > 0);
	char string[size + 1];
	snprintf(string, sizeof(string), "%d", uniqueName);
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

static int GetUniqueName(const IdentifierReference* identifier)
{
	switch (identifier->reference.type)
	{
	case Node_VariableDeclaration: return ((VarDeclStmt*)identifier->reference.ptr)->uniqueName;
	case Node_FunctionDeclaration: return ((FuncDeclStmt*)identifier->reference.ptr)->uniqueName;
	default: INVALID_VALUE(identifier->reference.type);
	}
}

static bool IsExternal(const IdentifierReference* identifier)
{
	switch (identifier->reference.type)
	{
	case Node_VariableDeclaration: return ((VarDeclStmt*)identifier->reference.ptr)->external;
	case Node_FunctionDeclaration: return ((FuncDeclStmt*)identifier->reference.ptr)->external;
	default: INVALID_VALUE(identifier->reference.type);
	}
}

static void VisitExpression(NodePtr node, const NodePtr* parentExpr);
static void VisitStatement(const NodePtr* node);

static void VisitFunctionCall(FuncCallExpr* funcCall)
{
	WriteString(funcCall->identifier.text);
	if (!IsExternal(&funcCall->identifier))
	{
		WriteChar('_');
		WriteUniqueName(GetUniqueName(&funcCall->identifier));
	}

	NodePtr funcCallNode = (NodePtr){.ptr = funcCall, .type = Node_FunctionCall};

	WriteChar('(');
	for (size_t i = 0; i < funcCall->arguments.length; ++i)
	{
		VisitExpression(*(NodePtr*)funcCall->arguments.array[i], &funcCallNode);

		if (i < funcCall->arguments.length - 1)
			WriteString(", ");
	}
	WriteChar(')');
}

static void VisitLiteralExpression(LiteralExpr* literal)
{
	switch (literal->type)
	{
	case Literal_Float: WriteString(literal->floatValue); break;
	case Literal_Int: WriteUInt64(literal->intValue); break;
	case Literal_Identifier:
		WriteString(literal->identifier.text);
		if (!IsExternal(&literal->identifier))
		{
			WriteChar('_');
			WriteUniqueName(GetUniqueName(&literal->identifier));
		}
		break;
	case Literal_String:
		WriteChar('\"');
		WriteString(literal->string);
		WriteChar('\"');
		break;
	default: INVALID_VALUE(literal->type);
	}
}

static void VisitBinaryExpression(BinaryExpr* binary, const NodePtr* parentExpr)
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

	default: INVALID_VALUE(binary->operatorType);
	}

	bool writeBrackets = true;
	if (parentExpr == NULL)
		writeBrackets = false;
	else if (parentExpr->type == Node_Binary)
	{
		BinaryExpr* parent = parentExpr->ptr;
		if (binaryPrecedence[binary->operatorType] >= binaryPrecedence[parent->operatorType])
			writeBrackets = false;
	}

	NodePtr binaryNode = (NodePtr){.ptr = binary, .type = Node_Binary};

	if (writeBrackets) WriteChar('(');
	VisitExpression(binary->left, &binaryNode);
	WriteChar(' ');
	WriteString(operator);
	WriteChar(' ');
	VisitExpression(binary->right, &binaryNode);
	if (writeBrackets) WriteChar(')');
}

static void VisitUnaryExpression(UnaryExpr* unary)
{
	char* operator;
	switch (unary->operatorType)
	{
	case Unary_Plus: operator= "+"; break;
	case Unary_Minus: operator= "-"; break;
	case Unary_Negate: operator= "!"; break;
	default: INVALID_VALUE(unary->operatorType);
	}

	WriteString(operator);
	VisitExpression(unary->expression, &(NodePtr){.ptr = unary, .type = Node_Unary});
}

static void VisitSubscript(SubscriptExpr* subscript)
{
	WriteString(subscript->identifier.text);
	WriteChar('_');
	WriteUniqueName(GetUniqueName(&subscript->identifier));

	WriteChar('[');
	VisitExpression(subscript->expr, &(NodePtr){.ptr = subscript, .type = Node_Subscript});
	WriteChar(']');
}

static void VisitExpression(const NodePtr node, const NodePtr* parentExpr)
{
	switch (node.type)
	{
	case Node_Binary: VisitBinaryExpression(node.ptr, parentExpr); break;
	case Node_Unary: VisitUnaryExpression(node.ptr); break;
	case Node_Literal: VisitLiteralExpression(node.ptr); break;
	case Node_FunctionCall: VisitFunctionCall(node.ptr); break;
	case Node_Subscript: VisitSubscript(node.ptr); break;
	default: INVALID_VALUE(node.type);
	}
}

static void VisitVariableDeclaration(VarDeclStmt* varDecl)
{
	if (varDecl->external)
		return;

	assert(varDecl->initializer.ptr != NULL);

	VisitBinaryExpression(
		&(BinaryExpr){
			.lineNumber = -1,
			.operatorType = Binary_Assignment,
			.right = varDecl->initializer,
			.left = (NodePtr){
				&(LiteralExpr){
					.lineNumber = -1,
					.type = Literal_Identifier,
					.identifier = (IdentifierReference){
						.text = varDecl->name,
						.reference = (NodePtr){.ptr = varDecl, .type = Node_VariableDeclaration},
					},
				},
				Node_Literal},
		},
		NULL);
	WriteString(";\n");
}

static void VisitBlock(const BlockStmt* block, const bool semicolon)
{
	WriteString("(\n");
	PushIndent();
	for (size_t i = 0; i < block->statements.length; ++i)
		VisitStatement(block->statements.array[i]);
	PopIndent();
	WriteChar(')');
	if (semicolon) WriteString(";\n");
}

static void VisitFunctionDeclaration(const FuncDeclStmt* funcDecl)
{
	if (funcDecl->external)
		return;

	WriteString("function ");
	WriteString(funcDecl->name);
	WriteChar('_');
	WriteUniqueName(funcDecl->uniqueName);

	WriteChar('(');
	for (size_t i = 0; i < funcDecl->parameters.length; ++i)
	{
		const NodePtr* node = funcDecl->parameters.array[i];

		assert(node->type == Node_VariableDeclaration);
		const VarDeclStmt* varDecl = node->ptr;
		WriteString(varDecl->name);
		WriteChar('_');
		WriteUniqueName(varDecl->uniqueName);

		if (i < funcDecl->parameters.length - 1)
			WriteString(", ");
	}
	WriteString(")\n");

	assert(funcDecl->block.type == Node_BlockStatement);
	VisitBlock(funcDecl->block.ptr, true);
}

static void VisitIfStatement(const IfStmt* ifStmt)
{
	VisitExpression(ifStmt->expr, NULL);
	WriteString(" ?\n");

	assert(ifStmt->trueStmt.type == Node_BlockStatement);
	VisitBlock(ifStmt->trueStmt.ptr, false);

	if (ifStmt->falseStmt.ptr != NULL)
	{
		WriteString(" : ");
		assert(ifStmt->falseStmt.type == Node_BlockStatement);
		VisitBlock(ifStmt->falseStmt.ptr, false);
	}

	WriteString(";\n");
}

static void VisitWhileStatement(const WhileStmt* whileStmt)
{
	WriteString("while (");
	VisitExpression(whileStmt->expr, NULL);
	WriteString(")\n");

	assert(whileStmt->stmt.type == Node_BlockStatement);
	VisitBlock(whileStmt->stmt.ptr, true);
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
		VisitExpression(expressionStmt->expr, NULL);
		WriteString(";\n");
		break;
	case Node_BlockStatement:
		VisitBlock(node->ptr, true);
		break;
	case Node_If:
		VisitIfStatement(node->ptr);
		break;
	case Node_While:
		VisitWhileStatement(node->ptr);
		break;
	case Node_Null:
		break;
	default: INVALID_VALUE(node->type);
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
	default: INVALID_VALUE(section->sectionType);
	}

	WriteChar('@');
	WriteString(sectionText);
	WriteChar('\n');

	assert(section->block.type == Node_BlockStatement);
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
		case Node_Null:
			break;
		default: INVALID_VALUE(stmt->type);
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
