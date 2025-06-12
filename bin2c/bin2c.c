#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

#define ASSERT(x)                                          \
	do                                                     \
	{                                                      \
		if (!(x))                                          \
		{                                                  \
			fprintf(stderr, "Assertion failed: %s\n", #x); \
			abort();                                       \
		}                                                  \
	}                                                      \
	while (0)

struct Buffer
{
	char* ptr;
	size_t length;
};

static char* AllocFileNameNoExtension(const char* path)
{
	ASSERT(path);

	size_t length = strlen(path);
	ASSERT(length != 0);
	size_t start = 0;
	size_t end = length;
	for (size_t i = 0; i < length; ++i)
	{
		if (path[i] == '/' || path[i] == '\\')
			start = i + 1;
		if (path[i] == '.')
			end = i;
	}

	if (start >= length)
	{
		fprintf(stderr, "%s is a directory, not a file", path);
		exit(EXIT_FAILURE);
	}

	if (end < start)
		end = length;

	size_t newLength = end - start;
	if (newLength == 0)
	{
		fprintf(stderr, "Invalid file name: %s", path);
		exit(EXIT_FAILURE);
	}

	char* new = malloc(newLength + 1);
	ASSERT(new);
	memcpy(new, path + start, newLength);
	new[newLength] = '\0';
	return new;
}

static struct Buffer ReadFile(const char* path)
{
	ASSERT(path);

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
	struct Buffer buffer = (struct Buffer){
		.ptr = malloc((size_t)fileSize),
		.length = (size_t)fileSize,
	};
	if (!buffer.ptr)
		goto error;

	errno = 0;
	if (fread(buffer.ptr, sizeof(char), (size_t)fileSize, file) < (size_t)fileSize)
		goto error;

	fclose(file);
	return buffer;

error:
	fprintf(stderr, "Failed to read file: %s (%s)\n", strerror(errno), path);
	exit(EXIT_FAILURE);
}

static void Write(FILE* file, const char* format, ...)
{
	ASSERT(file);
	ASSERT(format);

	va_list args;

	errno = 0;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	if (ferror(file) != 0)
	{
		fprintf(stderr, "Failed to write to file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, const char** argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "Incorrect number of arguments: %d\n", argc - 1);
		return EXIT_FAILURE;
	}

	errno = 0;
	FILE* outFile = fopen(argv[1], "w");
	if (!outFile)
	{
		fprintf(stderr, "Failed to open output file: %s (%s)\n", strerror(errno), argv[1]);
		return EXIT_FAILURE;
	}

	Write(outFile, "#include <stddef.h>\n");
	for (int i = 2; i < argc; ++i)
	{
		struct Buffer buffer = ReadFile(argv[i]);
		char* name = AllocFileNameNoExtension(argv[i]);

		Write(outFile, "\n");

		Write(outFile, "const char %s[] = {", name);
		for (size_t j = 0; j < buffer.length; ++j)
			Write(outFile, "%#x, ", buffer.ptr[j]);
		Write(outFile, "};\n");

		Write(outFile, "const size_t %s_length = %zu;\n", name, buffer.length);

		free(name);
		free(buffer.ptr);
	}

	fclose(outFile);
	return EXIT_SUCCESS;
}
