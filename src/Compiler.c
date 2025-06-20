#include "Compiler.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PlatformUtils.h"
#include "Parser.h"
#include "Scanner.h"
#include "StringUtils.h"
#include "code-generation/CodeGenerator.h"
#include "data-structures/Array.h"
#include "BuiltIn.h"

#define STRINGIFY(x) #x

typedef struct
{
	AST ast;
	Array dependencies;
	char* path;
	char* moduleName;
	bool isBuiltIn;
	bool searched;
} ProgramNode;

typedef struct
{
	ProgramNode* node;
	int importLineNumber;
	bool publicImport;
} ProgramDependency;

static void FreeProgramTree(const Array* programNodes)
{
	for (size_t i = 0; i < programNodes->length; ++i)
	{
		ProgramNode* node = *(ProgramNode**)programNodes->array[i];
		FreeArray(&node->dependencies);
		free(node->path);
		free(node->moduleName);
		free(node);
	}
	FreeArray(programNodes);
}

static Result ReadFile(const char* path, char** outString, size_t* outStringLength, int lineNumber, const char* errorPath)
{
	ASSERT(path);
	ASSERT(outString);
	ASSERT(outStringLength);

	errno = 0;
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		goto error;

	errno = 0;
	if (fseek(file, 0, SEEK_END) != 0)
		goto error;

	errno = 0;
	long fileSize = ftell(file);
	if (fileSize < 0)
		goto error;

	rewind(file);

	errno = 0;
	*outString = malloc((size_t)fileSize);
	*outStringLength = (size_t)fileSize;
	if (!*outString)
		goto error;

	errno = 0;
	if (fread(*outString, sizeof(char), (size_t)fileSize, file) < (size_t)fileSize)
		goto error;

	fclose(file);
	return SUCCESS_RESULT;

error:
	return ERROR_RESULT(
		errno != 0
			? AllocateString2Str(
				  "Failed to read file \"%s\": %s",
				  path,
				  strerror(errno))
			: AllocateString1Str(
				  "Failed to read file \"%s\"",
				  path),
		lineNumber,
		errorPath);
}

static Result WriteFile(const char* path, const char* bytes, size_t bytesLength)
{
	ASSERT(path);

	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL)
		goto error;

	if (fwrite(bytes, sizeof(char), bytesLength, file) < bytesLength)
		goto error;

	fclose(file);
	return SUCCESS_RESULT;

error:
	return ERROR_RESULT(
		errno != 0
			? AllocateString2Str(
				  "Failed to write file \"%s\": %s",
				  path,
				  strerror(errno))
			: AllocateString1Str(
				  "Failed to write file \"%s\"",
				  path),
		-1,
		NULL);
}

static Result CheckFileReadable(const char* path, int lineNumber, const char* errorPath)
{
	char* errorMessage = NULL;
	ASSERT(path);

	errno = 0;
	int result = IsRegularFile(path);
	if (result == 0)
		goto error_is_not_file;
	else if (result != 1)
		goto error;

	errno = 0;
	if (!CheckFileAccess(path, false, true))
		goto error;

	errno = 0;
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		goto error;

	fclose(file);
	return SUCCESS_RESULT;

error_is_not_file:
	if (!errorMessage)
		errorMessage = "Path is not a regular file";

error:
	if (!errorMessage && errno != 0)
		errorMessage = strerror(errno);

	return ERROR_RESULT(
		errorMessage
			? AllocateString2Str(
				  "Failed to read file \"%s\": %s",
				  path,
				  errorMessage)
			: AllocateString1Str(
				  "Failed to read file \"%s\"",
				  path),
		lineNumber,
		errorPath);
}

static Result CheckFileWriteable(const char* path, int lineNumber, const char* errorPath)
{
	char* errorMessage = NULL;
	ASSERT(path);

	errno = 0;
	FILE* file = fopen(path, "wb");
	if (file == NULL)
		goto error;

	errno = 0;
	int result = IsRegularFile(path);
	if (result == 0)
		goto error_is_not_file;
	else if (result != 1)
		goto error;

	errno = 0;
	if (!CheckFileAccess(path, false, true))
		goto error;

	fclose(file);
	return SUCCESS_RESULT;

error_is_not_file:
	if (!errorMessage)
		errorMessage = "Path is not a regular file";

error:
	if (!errorMessage && errno != 0)
		errorMessage = strerror(errno);

	return ERROR_RESULT(
		errorMessage
			? AllocateString2Str(
				  "Failed to write file \"%s\": %s",
				  path,
				  errorMessage)
			: AllocateString1Str(
				  "Failed to write file \"%s\"",
				  path),
		lineNumber,
		errorPath);
}

static Result CheckForCircularDependency(const ProgramNode* node, const ProgramNode* importedNode, const int lineNumber, const char* errorPath)
{
	for (size_t i = 0; i < importedNode->dependencies.length; ++i)
	{
		const ProgramDependency* dependency = importedNode->dependencies.array[i];

		if (dependency->node == node)
			return ERROR_RESULT("Circular dependency detected", lineNumber, errorPath);

		PROPAGATE_ERROR(CheckForCircularDependency(node, dependency->node, lineNumber, errorPath));
	}

	return SUCCESS_RESULT;
}

static Result CheckForModuleNameConflict(const char* moduleName, const Array* programNodes, int lineNumber, const char* errorPath)
{
	for (size_t i = 0; i < programNodes->length; ++i)
	{
		const ProgramNode* p = *(ProgramNode**)programNodes->array[i];
		if (strcmp(p->moduleName, moduleName) == 0)
			return ERROR_RESULT(
				AllocateString1Str(
					"Module \"%s\" is already defined",
					moduleName),
				lineNumber,
				errorPath);
	}
	return SUCCESS_RESULT;
}

static bool ChangeDirectoryToFileName(const char* fileName)
{
	char* dirName = AllocDirectoryName(fileName);
	if (!dirName)
		return false;

	if (!ChangeDirectory(dirName))
		return false;

	free(dirName);
	return true;
}

static ProgramNode* GenerateBuiltInProgramNode(Array* programNodes, const char* moduleName, const char* source, size_t sourceLength)
{
	// if it already exists return the existing one
	for (size_t i = 0; i < programNodes->length; ++i)
	{
		ProgramNode* programNode = *(ProgramNode**)programNodes->array[i];
		if (strcmp(programNode->moduleName, moduleName) == 0)
			return programNode;
	}

	ProgramNode* thisProgramNode = malloc(sizeof(ProgramNode));
	*thisProgramNode = (ProgramNode){
		.path = AllocateString(moduleName),
		.moduleName = AllocateString(moduleName),
		.dependencies = AllocateArray(sizeof(ProgramDependency)),
		.isBuiltIn = true,
	};

	ArrayAdd(programNodes, &thisProgramNode);

	Array tokens;
	Result result = Scan(thisProgramNode->path, source, sourceLength, &tokens);
	ASSERT(result.type == Result_Success);
	result = Parse(thisProgramNode->path, &tokens, &thisProgramNode->ast);
	ASSERT(result.type == Result_Success);
	FreeArray(&tokens);
	return thisProgramNode;
}

static void AddBuiltInDependency(AST* ast, Array* dependencies, Array* programNodes, const char* moduleName, const char* source, size_t sourceLength)
{
	NodePtr node = AllocASTNode(
		&(ImportStmt){
			.lineNumber = -1,
			.path = AllocateString(moduleName),
			.moduleName = AllocateString(moduleName),
			.builtIn = true,
		},
		sizeof(ImportStmt), Node_Import);
	ArrayInsert(&ast->nodes, &node, 0);

	ArrayAdd(dependencies, &(ProgramDependency){
							   .node = GenerateBuiltInProgramNode(programNodes, moduleName, source, sourceLength),
							   .importLineNumber = -1,
						   });
}

static void AddBuiltInDependencies(AST* ast, Array* dependencies, Array* programNodes)
{
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(jsfx), jsfx, jsfx_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(math), math, math_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(str), str, str_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(gfx), gfx, gfx_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(time), time, time_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(file), file, file_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(mem), mem, mem_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(stack), stack, stack_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(atomic), atomic, atomic_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(slider), slider, slider_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(midi), midi, midi_length);
	AddBuiltInDependency(ast, dependencies, programNodes, STRINGIFY(pin_mapper), pin_mapper, pin_mapper_length);
}

static Result GenerateProgramNode(
	Array* programNodes,
	const char* path,
	int containingLineNumber,
	const char* containingPath,
	ProgramNode** outProgramNode)
{
	PROPAGATE_ERROR(CheckFileReadable(path, containingLineNumber, containingPath));

	ProgramNode* thisProgramNode = malloc(sizeof(ProgramNode));
	{
		char* absolutePath = AllocAbsolutePath(path);
		ASSERT(absolutePath);
		char* moduleName = AllocFileNameNoExtension(path);
		ASSERT(moduleName);
		*thisProgramNode = (ProgramNode){
			.path = absolutePath,
			.moduleName = moduleName,
		};
	}

	for (size_t i = 0; i < programNodes->length; ++i)
	{
		ProgramNode* node = *(ProgramNode**)programNodes->array[i];
		if (node->isBuiltIn)
			continue;

		int result = IsSameFile(thisProgramNode->path, node->path);
		ASSERT(result != -1);

		if (result)
		{
			if (outProgramNode)
				*outProgramNode = node;

			free(thisProgramNode->path);
			free(thisProgramNode->moduleName);
			free(thisProgramNode);
			return SUCCESS_RESULT;
		}
	}

	char* source = NULL;
	size_t sourceLength = 0;
	PROPAGATE_ERROR(ReadFile(thisProgramNode->path, &source, &sourceLength, containingLineNumber, containingPath));

	Array tokens;
	PROPAGATE_ERROR(Scan(thisProgramNode->path, source, sourceLength, &tokens));
	PROPAGATE_ERROR(Parse(thisProgramNode->path, &tokens, &thisProgramNode->ast));
	free(source);
	FreeArray(&tokens);

	thisProgramNode->dependencies = AllocateArray(sizeof(ProgramDependency));
	AddBuiltInDependencies(&thisProgramNode->ast, &thisProgramNode->dependencies, programNodes);

	PROPAGATE_ERROR(CheckForModuleNameConflict(thisProgramNode->moduleName, programNodes, containingLineNumber, containingPath));
	ArrayAdd(programNodes, &thisProgramNode);

	for (size_t i = 0; i < thisProgramNode->ast.nodes.length; ++i)
	{
		const NodePtr* node = thisProgramNode->ast.nodes.array[i];
		if (node->type == Node_Modifier)
			continue;
		if (node->type != Node_Import)
			break;

		ImportStmt* importStmt = node->ptr;
		if (importStmt->builtIn)
			continue;

		ProgramNode* importProgramNode = NULL;
		if (strlen(importStmt->path) == 0)
			return ERROR_RESULT("Empty import statements are not allowed", importStmt->lineNumber, thisProgramNode->path);

		bool success = ChangeDirectoryToFileName(thisProgramNode->path);
		ASSERT(success);

		PROPAGATE_ERROR(GenerateProgramNode(programNodes, importStmt->path, importStmt->lineNumber, thisProgramNode->path, &importProgramNode));
		importStmt->moduleName = AllocateString(importProgramNode->moduleName);
		ProgramDependency dependency = {
			.node = importProgramNode,
			.importLineNumber = importStmt->lineNumber,
			.publicImport = importStmt->modifiers.publicValue,
		};
		ArrayAdd(&thisProgramNode->dependencies, &dependency);

		PROPAGATE_ERROR(CheckForCircularDependency(thisProgramNode, dependency.node, importStmt->lineNumber, thisProgramNode->path));
	}

	if (outProgramNode)
		*outProgramNode = thisProgramNode;
	return SUCCESS_RESULT;
}

typedef void (*ProgramTreeVisitFunc)(const ProgramNode*, void*);
static void ProgramTreeVisit(ProgramNode* node, const ProgramTreeVisitFunc func, void* data)
{
	if (node->searched == true)
		return;

	for (size_t i = 0; i < node->dependencies.length; ++i)
		ProgramTreeVisit(((ProgramDependency*)node->dependencies.array[i])->node, func, data);

	node->searched = true;
	func(node, data);
}

static void TopologicalVisitProgramTree(const Array* programNodes, const ProgramTreeVisitFunc func, void* data)
{
	for (size_t i = 0; i < programNodes->length; ++i)
	{
		ProgramNode* node = *(ProgramNode**)programNodes->array[i];
		if (node->searched == 0)
			ProgramTreeVisit(node, func, data);
	}
}

static void AddNodeToMergedAST(const ProgramNode* node, void* mergedAST)
{
	const NodePtr module = AllocASTNode(
		&(ModuleNode){
			.path = AllocateString(node->path),
			.moduleName = AllocateString(node->moduleName),
			.statements = node->ast.nodes,
		},
		sizeof(ModuleNode), Node_Module);
	AST* ast = mergedAST;
	ArrayAdd(&ast->nodes, &module);
}

static Result CompileProgramTree(const Array* programNodes, char** outCode, size_t* outLength)
{
	AST merged = {.nodes = AllocateArray(sizeof(NodePtr))};
	TopologicalVisitProgramTree(programNodes, AddNodeToMergedAST, &merged);
	PROPAGATE_ERROR(GenerateCode(&merged, outCode, outLength));
	FreeAST(merged);
	return SUCCESS_RESULT;
}

Result Compile(const char* inputPath, const char* outputPath)
{
	PROPAGATE_ERROR(CheckFileWriteable(outputPath, -1, NULL));
	char* outPath = AllocAbsolutePath(outputPath);

	Array programNodes = AllocateArray(sizeof(ProgramNode*));
	PROPAGATE_ERROR(GenerateProgramNode(&programNodes, inputPath, -1, NULL, NULL));

	char* code = NULL;
	size_t codeLength = 0;
	PROPAGATE_ERROR(CompileProgramTree(&programNodes, &code, &codeLength));
	FreeProgramTree(&programNodes);

	PROPAGATE_ERROR(WriteFile(outPath, code, codeLength));

	free(outPath);
	free(code);
	return SUCCESS_RESULT;
}
