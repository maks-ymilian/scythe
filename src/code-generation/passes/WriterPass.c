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

static void VisitExpression(const NodePtr node)
{
	WriteString("expression\n");
}

static void VisitVariableDeclaration(const VarDeclStmt* varDecl)
{
	WriteString("variable\n");
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
