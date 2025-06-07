#include <stdio.h>

#include "Compiler.h"
#include "PlatformUtils.h"

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
		ASSERT(result.type == Result_Error);
		ASSERT(result.errorMessage != NULL);
		fprintf(stderr, "ERROR: %s", result.errorMessage);
		if (result.lineNumber > 0) fprintf(stderr, " (line %d)", result.lineNumber);
		if (result.filePath != NULL) fprintf(stderr, " (%s)", result.filePath);
		fprintf(stderr, "\n");

		fprintf(stderr, "Compilation failed.\n");
		return EXIT_FAILURE;
	}
}
