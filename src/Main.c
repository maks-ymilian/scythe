#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "Scanner.h"
#include "Parser.h"
#include "data-structures/Array.h"
#include "code-generation/CodeGenerator.h"
#include "tests/TestGenerator.h"

#define HANDLE_ERROR(func, message)\
{\
Result result = func;\
if (result.hasError)\
{\
	printf(message": ");\
	PrintError(result);\
	printf("Press ENTER to continue...\n");\
	getchar();\
	exit(1);\
}else assert(result.success);\
}

static void WriteOutputFile(const char* path, const char* code, const size_t length)
{
    printf("Output code:\n%.*s", (int)length, code);

    FILE* file = fopen(path, "wb");
    if (file == NULL)
    {
        printf("Failed to open output file %s: %s\n", path, strerror(errno));
        exit(1);
    }
    const size_t bytesWritten = fwrite(code, sizeof(uint8_t), length, file);
    if (bytesWritten < length)
    {
        printf("An error occurred when writing to file %s\nBytes written: %llu\nTotal bytes: %llu", path, bytesWritten, length);
        exit(1);
    }
    fclose(file);
}

static char* Compile(const char* source, size_t* length)
{
    Array tokens;
    HANDLE_ERROR(Scan(source, &tokens), "Scan error");

    NodePtr syntaxTree;
    HANDLE_ERROR(Parse(&tokens, &syntaxTree), "Parse error");

    uint8_t* outputCode = NULL;
    size_t outputCodeLength = 0;
    HANDLE_ERROR(GenerateCode(syntaxTree.ptr, &outputCode, &outputCodeLength), "Code generation error");
    assert(outputCode != NULL);

    if (length != NULL)
        *length = outputCodeLength;

    return (char*)outputCode;
}

static void CompileFile(const char* inputPath, const char* outputPath)
{
    FILE* file = fopen(inputPath, "rb");
    if (file == NULL)
    {
        printf("Failed to open input file %s: %s\n", inputPath, strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    assert(fileSize != -1L);
    rewind(file);

    char* string = malloc(fileSize + 1);
    assert(string != NULL);

    fread(string, (size_t)fileSize, 1, file);
    fclose(file);
    string[fileSize] = '\0';

    size_t outputLength = 0;
    char* outputCode = Compile(string, &outputLength);
    WriteOutputFile(outputPath, outputCode, outputLength);

    free(outputCode);
    free(string);
}

#define RUN_TEST

#ifdef RUN_TEST
int main(void)
{
    const char* testSource = GenerateTestSource();

    size_t outputLength = 0;
    char* outputSource = Compile(testSource, &outputLength);

    WriteOutputFile("C:/REAPER/Effects/scythe/test/test.jsfx", outputSource, outputLength);
    free(outputSource);

    system("\"C:/REAPER/reaper.exe\" -nonewinst C:/REAPER/Effects/scythe/test/run_test.lua");

    return 0;
}
#endif

#ifndef RUN_TEST
int main(const int argc, const char** argv)
{
    if (argc < 2 || argc > 3)
    {
        printf("Usage: scythe <input_path> [output_path]\n");
        return 1;
    }

    const char* inputPath = argv[1];

    const char* outputPath = "out.jsfx";
    if (argc == 3) outputPath = argv[2];

    CompileFile(inputPath, outputPath);

    return 0;
}
#endif
