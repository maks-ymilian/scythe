#include <stddef.h>
#include <stdio.h>

#include "Compiler.h"
#include "FileUtils.h"

int main(int argc, char** argv)
{
	if (argc < 2 || argc > 3)
	{
		printf("Usage: %s <input_file> [output_file]\n", AllocFileName(argv[0]));
		return 1;
	}

	const char* inputPath = argv[1];

	const char* outputPath = "out.jsfx";
	if (argc == 3) outputPath = argv[2];

	Compile(inputPath, outputPath);

	return 0;
}
