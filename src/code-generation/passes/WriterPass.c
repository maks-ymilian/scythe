#include "WriterPass.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Result.h"
#include "SyntaxTree.h"
#include "data-structures/MemoryStream.h"

static MemoryStream* stream;

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

static void WriteString(const char* str) { StreamWrite(stream, str, strlen(str)); }
static void WriteChar(const char chr) { StreamWriteByte(stream, (uint8_t)chr); }

static void VisitLiteralExpression(const LiteralExpr* literal)
{
	switch (literal->type)
	{
	case Literal_Float: WriteString(literal->floatValue); break;
	case Literal_Int: WriteInteger(literal->intValue); break;
	case Literal_Identifier: WriteString(literal->identifier.text); break;
	default: unreachable();
	}
}

static void VisitExpression(NodePtr node);

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

	case Binary_Add: operator= "+"; break;
	case Binary_Subtract: operator= "-"; break;
	case Binary_Multiply: operator= "*"; break;
	case Binary_Divide: operator= "/"; break;

	case Binary_Assignment: operator= "="; break;
	case Binary_AddAssign: operator= "+="; break;
	case Binary_SubtractAssign: operator= "-="; break;
	case Binary_MultiplyAssign: operator= "*="; break;
	case Binary_DivideAssign: operator= "/="; break;

	default: unreachable();
	}

	VisitExpression(binary->left);
	WriteChar(' ');
	WriteString(operator);
	WriteChar(' ');
	VisitExpression(binary->right);
}

static void VisitExpression(const NodePtr node)
{
	switch (node.type)
	{
	case Node_Binary: VisitBinaryExpression(node.ptr); break;
	case Node_Literal:
		VisitLiteralExpression(node.ptr);
		break;

		// temporary
	case Node_MemberAccess:
		const MemberAccessExpr* memberAccess = node.ptr;
		assert(memberAccess->value.type == Node_Literal);
		VisitLiteralExpression(memberAccess->value.ptr);
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

static void VisitSection(const SectionStmt* section)
{
	const char* sectionText = NULL;
	switch (section->sectionType)
	{
	case Section_Init:
		sectionText = "init";
		break;
	case Section_Block:
		sectionText = "block";
		break;
	case Section_Sample:
		sectionText = "sample";
		break;
	case Section_Serialize:
		sectionText = "serialize";
		break;
	case Section_Slider:
		sectionText = "slider";
		break;
	case Section_GFX:
		sectionText = "gfx";
		break;
	default: unreachable();
	}

	WriteChar('@');
	WriteString(sectionText);
	WriteChar('\n');

	assert(section->block.type == Node_Block);
	const BlockStmt* block = section->block.ptr;
	for (size_t i = 0; i < block->statements.length; ++i)
	{
		const NodePtr* stmt = block->statements.array[i];
		switch (stmt->type)
		{
		case Node_FunctionDeclaration:
			WriteString("function\n");
			break;
		case Node_VariableDeclaration:
			VisitVariableDeclaration(stmt->ptr);
			break;
		case Node_ExpressionStatement:
			const ExpressionStmt* expressionStmt = stmt->ptr;
			VisitExpression(expressionStmt->expr);
			WriteString(";\n");
			break;

			// temporary
		case Node_Block:
			break;
		case Node_StructDeclaration:
			WriteString("struct\n");
			break;
		default: unreachable();
		}
	}

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

void WriterPass(const AST* ast, char** outBuffer, size_t* outLength)
{
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
