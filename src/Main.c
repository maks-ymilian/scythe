#include <libgen.h>
#include <stdio.h>

#include "Compiler.h"

int main(const int argc, char** argv)
{
	if (argc < 2 || argc > 3)
	{
		printf("Usage: %s <input_path> [output_path]\n", basename(argv[0]));
		return 1;
	}

	const char* inputPath = argv[1];

	const char* outputPath = "out.jsfx";
	if (argc == 3) outputPath = argv[2];

	Compile(inputPath, outputPath);

	return 0;
}
