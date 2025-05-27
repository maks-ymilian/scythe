#include <stdio.h>
#include <assert.h>

#include "Compiler.h"
#include "FileUtils.h"

int main(int argc, char** argv)
{
	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "Usage: %s <input_file> [output_file]\n", AllocFileName(argv[0]));
		return EXIT_FAILURE;
	}

	const char* inputPath = argv[1];

	const char* outputPath = "out.jsfx";
	if (argc == 3) outputPath = argv[2];

	Result result = Compile(inputPath, outputPath);
	if (result.type == Result_Success)
	{
		printf("Successfully compiled to output file: %s\n", outputPath);
		return EXIT_SUCCESS;
	}
	else
	{
		assert(result.type == Result_Error);
		assert(result.errorMessage != NULL);
		printf("ERROR: %s", result.errorMessage);
		if (result.lineNumber > 0) printf(" (line %d)", result.lineNumber);
		if (result.filePath != NULL) printf(" (%s)", result.filePath);
		printf("\n");

		printf("Compilation failed.\n");
		return EXIT_FAILURE;
	}
}
