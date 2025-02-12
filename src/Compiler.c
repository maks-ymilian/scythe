#include "Compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libgen.h>
#include <unistd.h>

#include "BuiltInImports.h"
#include "FileUtils.h"
#include "Parser.h"
#include "Scanner.h"
#include "StringUtils.h"
#include "code-generation/CodeGenerator.h"
#include "data-structures/Array.h"
#include "data-structures/MemoryStream.h"

static const char* watermark =
	"\n"
	"      generated by                 /$$     /$$                \n"
	"                                  | $$    | $$                \n"
	"    /$$$$$$$  /$$$$$$$ /$$   /$$ /$$$$$$  | $$$$$$$   /$$$$$$ \n"
	"   /$$_____/ /$$_____/| $$  | $$|_  $$_/  | $$__  $$ /$$__  $$\n"
	"  |  $$$$$$ | $$      | $$  | $$  | $$    | $$  \\ $$| $$$$$$$$\n"
	"   \\____  $$| $$      | $$  | $$  | $$ /$$| $$  | $$| $$_____/\n"
	"   /$$$$$$$/|  $$$$$$$|  $$$$$$$  |  $$$$/| $$  | $$|  $$$$$$$\n"
	"  |_______/  \\_______/ \\____  $$   \\___/  |__/  |__/ \\_______/\n"
	"                       /$$  | $$                              \n"
	"                      |  $$$$$$/                              \n"
	"                       \\______/                               \n\n";

typedef struct
{
	AST ast;
	Array dependencies;
	char* path;
	char* moduleName;
	bool builtIn;
	bool searched;
} ProgramNode;

typedef struct
{
	ProgramNode* node;
	int importLineNumber;
	bool publicImport;
} ProgramDependency;

static char* GetFileString(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		return NULL;

	fseek(file, 0, SEEK_END);
	const long fileSize = ftell(file);
	assert(fileSize >= 0);
	rewind(file);

	char* string = malloc((size_t)fileSize + 1);

	fread(string, sizeof(char), (size_t)fileSize, file);
	fclose(file);
	string[fileSize] = '\0';
	return string;
}

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

static void HandleError(const char* errorStage, const char* filePath, Result result)
{
	if (result.type == Result_Success)
		return;

	if (filePath != NULL)
		result.filePath = filePath;

	if  (errorStage != NULL)
		printf("%s error: ", errorStage);

	PrintError(result);
	printf("\n");

	printf("Press ENTER to continue...\n");
	getchar();
	exit(1);
}

static Result WriteOutputFile(FILE* file, const char* path, const char* code, const size_t length)
{
	if (file == NULL)
	{
		fclose(file);
		return ERROR_RESULT(AllocateString2Str("Failed to open output file \"%s\": %s", path, strerror(errno)), -1, NULL);
	}

	if (length == 0)
	{
		fclose(file);
		return SUCCESS_RESULT;
	}

	fwrite(watermark, sizeof(uint8_t), strlen(watermark), file);
	fwrite(code, sizeof(uint8_t), length, file);
	if (ferror(file))
		return ERROR_RESULT(AllocateString1Str("Failed to write to output file \"%s\"", path), -1, NULL);

	fclose(file);
	return SUCCESS_RESULT;
}

static Result IsFileOpenable(const char* path, const int lineNumber)
{
	FILE* file = fopen(path, "rb");

	if (file == NULL)
		return ERROR_RESULT(
			AllocateString2Str("Failed to open input file \"%s\": %s", path, strerror(errno)),
			lineNumber, NULL);

	fclose(file);

	return SUCCESS_RESULT;
}

static Result GetSourceFromImportPath(const char* path, const int lineNumber, char** outString)
{
	*outString = GetFileString(path);
	if (*outString == NULL)
		return ERROR_RESULT(
			AllocateString2Str("Failed to open input file \"%s\": %s", path, strerror(errno)),
			lineNumber, NULL);

	return SUCCESS_RESULT;
}

static Result CheckForCircularDependency(const ProgramNode* node, const ProgramNode* importedNode, const int lineNumber)
{
	for (size_t i = 0; i < importedNode->dependencies.length; ++i)
	{
		const ProgramDependency* dependency = importedNode->dependencies.array[i];

		if (dependency->node == node)
			return ERROR_RESULT("Circular dependency detected", lineNumber, NULL);

		PROPAGATE_ERROR(CheckForCircularDependency(node, dependency->node, lineNumber));
	}

	return SUCCESS_RESULT;
}

static bool IsSameFileOrBuiltInPath(
	const bool isBuiltIn,
	const char* path,
	const ProgramNode* otherNode,
	const int lineNumber,
	const char* containingPath)
{
	if (isBuiltIn || otherNode->builtIn)
		return strcmp(path, otherNode->path) == 0;

	const int isSameFile = IsSameFile(path, otherNode->path);
	if (isSameFile == -1)
		HandleError("Import", containingPath, ERROR_RESULT("Could not compare file paths", lineNumber, NULL));
	return isSameFile;
}

static char* AllocBaseFileName(const char* path)
{
	const size_t length = strlen(path) + 1;
	char string[length + 1];
	memcpy(string, path, length);

	char* fileName = basename(string);

	const size_t baseNameLength = strlen(fileName) + 1;
	for (size_t i = 0; i < baseNameLength; ++i)
		if (fileName[i] == '.') fileName[i] = '\0';

	return AllocateString(fileName);
}

static void AddBuiltInImportStatement(AST* ast)
{
	const NodePtr builtInImport = AllocASTNode(
		&(ImportStmt){
			.path = AllocateString("jsfx"),
			.moduleName = NULL,
			.public = false,
			.lineNumber = -1,
		},
		sizeof(ImportStmt), Node_Import);
	ArrayInsert(&ast->nodes, &builtInImport, 0);
}

static bool FindModuleNameConflict(const Array* programNodes, const ProgramNode* programNode)
{
	for (size_t i = 0; i < programNodes->length; ++i)
	{
		const ProgramNode* p = *(ProgramNode**)programNodes->array[i];
		if (strcmp(p->moduleName, programNode->moduleName) == 0)
			return true;
	}
	return false;
}

static ProgramNode* GenerateProgramNode(
	Array* programNodes,
	const char* path,
	const char* moduleName,
	int importLineNumber,
	const char* containingPath);

static void GenerateProgramNodeDependencies(Array* programNodes, ProgramNode* programNode, const char* path)
{
	programNode->dependencies = AllocateArray(sizeof(ProgramDependency));
	for (size_t i = 0; i < programNode->ast.nodes.length; ++i)
	{
		const NodePtr* node = programNode->ast.nodes.array[i];
		if (node->type != Node_Import) break;
		ImportStmt* importStmt = node->ptr;

		importStmt->moduleName = AllocBaseFileName(importStmt->path);

		ProgramDependency dependency =
			{
				.node = GenerateProgramNode(programNodes, importStmt->path, importStmt->moduleName, importStmt->lineNumber, path),
				.importLineNumber = importStmt->lineNumber,
				.publicImport = importStmt->public,
			};
		ArrayAdd(&programNode->dependencies, &dependency);

		HandleError("Import", path,
			CheckForCircularDependency(programNode, dependency.node, importStmt->lineNumber));
	}
}

static ProgramNode* GenerateProgramNode(
	Array* programNodes,
	const char* path,
	const char* moduleName,
	const int importLineNumber,
	const char* containingPath)
{
	const char* builtInSource = GetBuiltInSource(path);
	const bool isBuiltIn = builtInSource != NULL;

	if (!isBuiltIn)
		HandleError("Import", containingPath, IsFileOpenable(path, importLineNumber));

	for (size_t i = 0; i < programNodes->length; ++i)
	{
		ProgramNode* node = *(ProgramNode**)programNodes->array[i];
		if (IsSameFileOrBuiltInPath(isBuiltIn, path, node, importLineNumber, containingPath))
			return node;
	}

	ProgramNode* programNode = malloc(sizeof(ProgramNode));
	*programNode = (ProgramNode){
		.path = AllocateString(path),
		.moduleName = moduleName != NULL ? AllocateString(moduleName) : AllocBaseFileName(path),
		.builtIn = isBuiltIn,
		.searched = false,
	};
	if (FindModuleNameConflict(programNodes, programNode))
		HandleError("Import", path,
			ERROR_RESULT(AllocateString1Str("Module \"%s\" is already defined", programNode->moduleName),
				importLineNumber, NULL));
	ArrayAdd(programNodes, &programNode);

	char* source = NULL;
	if (!isBuiltIn)
		HandleError("Read", containingPath, GetSourceFromImportPath(path, importLineNumber, &source));

	Array tokens;
	HandleError("Scan", path, Scan(isBuiltIn ? builtInSource : source, &tokens));
	if (!isBuiltIn)
		free(source);

	HandleError("Parse", path, Parse(&tokens, &programNode->ast));
	FreeTokenArray(&tokens);

	// if (!isBuiltIn)
	// 	AddBuiltInImportStatement(&programNode->ast);

	GenerateProgramNodeDependencies(programNodes, programNode, path);

	return programNode;
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

static void CompileProgramTree(const Array* programNodes, char** outCode, size_t* outLength)
{
	AST merged = {.nodes = AllocateArray(sizeof(NodePtr))};
	TopologicalVisitProgramTree(programNodes, AddNodeToMergedAST, &merged);
	HandleError("Code generation", NULL, GenerateCode(&merged, outCode, outLength));
	FreeAST(merged);
}

static void ChangeDirectoryToFileName(const char* fileName)
{
	const size_t length = strlen(fileName);
	char copy[length + 1];
	memcpy(copy, fileName, length + 1);

	chdir(dirname(copy));
}

void Compile(const char* inputPath, const char* outputPath)
{
	FILE* outputFile = fopen(outputPath, "wb");

	ChangeDirectoryToFileName(inputPath);

	Array programNodes = AllocateArray(sizeof(ProgramNode*));
	GenerateProgramNode(&programNodes, inputPath, NULL, -1, NULL);

	char* outputCode = NULL;
	size_t outputCodeLength = 0;
	CompileProgramTree(&programNodes, &outputCode, &outputCodeLength);

	FreeProgramTree(&programNodes);

	printf("%.*s\n", (int)outputCodeLength, outputCode);
	HandleError("Write", NULL, WriteOutputFile(outputFile, outputPath, outputCode, outputCodeLength));

	free(outputCode);
}
