#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

#include "Compiler.h"

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

    size_t outputLength;
    const char* outputCode = Compile(inputPath, &outputLength);
    WriteOutputFile(outputPath, outputCode, outputLength);

    return 0;
}
