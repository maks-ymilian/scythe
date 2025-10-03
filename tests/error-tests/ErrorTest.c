#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>

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

#define COMMENT_TOKEN ((struct String){.ptr = "//", .length = 2})
#define ERROR_TOKEN ((struct String){.ptr = "<!>", .length = 3})

struct String
{
	char* ptr;
	size_t length;
};

static const char* currentFile;
static const char* currentBackupFile;

static int numTestsPassed;

static struct String AllocateString(const char* string, size_t length)
{
	char* new = malloc(length + 1);
	memcpy(new, string, length);
	new[length] = '\0';
	return (struct String){
		.ptr = new,
		.length = length,
	};
}

static struct String AllocateString2(const char* string)
{
	return AllocateString(string, strlen(string));
}

static struct String AllocFormat(const char* format, const char* string1, const char* string2)
{
	size_t string1Length = strlen(string1);
	size_t string2Length = strlen(string2);
	size_t length = strlen(format) + string1Length + string2Length;
	char* new = malloc(length + 1);
	snprintf(new, length, format, string1, string2);
	new[length] = '\0';
	return (struct String){
		.ptr = new,
		.length = length,
	};
}

static struct String ReadFile(FILE* file)
{
	struct String string;
	string.length = 64;
	string.ptr = malloc(string.length);

	errno = 0;
	char c;
	size_t i;
	for (i = 0; (c = (char)fgetc(file)) != EOF; ++i)
	{
		if (i >= string.length)
		{
			string.length *= 2;
			string.ptr = realloc(string.ptr, string.length);
		}

		string.ptr[i] = c;
	}
	if (!feof(file))
	{
		fprintf(stderr, "Failed to read file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	string.length = i;
	return string;
}

static void NextLine(FILE* file)
{
	while (true)
	{
		errno = 0;
		int c;
		if ((c = fgetc(file)) == EOF)
		{
			if (feof(file))
				return;
			else
				goto error;
		}

		if (c == '\n')
			return;
	}

error:
	fprintf(stderr, "Failed to read file: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

static struct String ReadCurrentLine(FILE* file)
{
	errno = 0;
	if (fgetc(file) == EOF)
	{
		if (feof(file))
			return (struct String){.ptr = NULL};
	}

	errno = 0;
	if (fseek(file, -1, SEEK_CUR) != 0)
		goto error;

	errno = 0;
	long pos = ftell(file);
	if (pos == -1L)
		goto error;

	while (true)
	{
		errno = 0;
		int c;
		if ((c = fgetc(file)) == EOF)
		{
			if (feof(file))
				break;
			else
				goto error;
		}

		if (c == '\n')
		{
			errno = 0;
			if (fseek(file, -1, SEEK_CUR))
				goto error;

			break;
		}
	}

	errno = 0;
	long lineBreakPos = ftell(file);
	if (lineBreakPos == -1L)
		goto error;

	errno = 0;
	if (fseek(file, pos, SEEK_SET) != 0)
		goto error;

	long length = lineBreakPos - pos;
	ASSERT(length >= 0);

	errno = 0;
	struct String string = (struct String){
		.ptr = malloc((size_t)length),
		.length = (size_t)length,
	};
	if (fread(string.ptr, sizeof(*string.ptr), (size_t)length, file) != (size_t)length)
		goto error;

	errno = 0;
	if (fseek(file, pos, SEEK_SET) != 0)
		goto error;

	return string;

error:
	fprintf(stderr, "Failed to read file: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

static void RunTest(const char* executable, const char* fileName, struct String expectedMessage, size_t lineNumber)
{
	struct String command = AllocFormat("%s %s 2>&1", executable, fileName);
	errno = 0;
	FILE* file = popen(command.ptr, "r");
	free(command.ptr);
	if (!file)
		goto error;

	struct String output = ReadFile(file);
	pclose(file);

	size_t prefixLength = sizeof("ERROR: ") - 1;
	size_t lastLinePos = 0;
	for (size_t i = 0; i < output.length; ++i)
	{
		if (output.ptr[i] == '\n' && i + prefixLength < output.length)
		{
			if (output.ptr[i + 1] == 'C') // ignore the extra line "Compilation failed." after the error
				break;
			lastLinePos = i + 1;
		}
	}

	prefixLength += lastLinePos;
	if (output.length - prefixLength < expectedMessage.length ||
		memcmp(output.ptr + prefixLength, expectedMessage.ptr, expectedMessage.length) != 0)
	{
		fprintf(stderr, "[x] Failed test in %s:%lu\n", fileName, lineNumber);
		fprintf(stderr, "Expected: %s\n", expectedMessage.ptr);
		fprintf(stderr, "Got: ");
		fwrite(output.ptr, sizeof(*output.ptr), output.length, stderr);
		exit(EXIT_FAILURE);
	}

	printf("[✓] %s (%s:%lu)\n", expectedMessage.ptr, fileName, lineNumber);
	free(output.ptr);
	++numTestsPassed;
	return;

error:
	fprintf(stderr, "Failed to run test: %s %s\n", strerror(errno), fileName);
	exit(EXIT_FAILURE);
}

static void CopyFile(const char* path, const char* newPath)
{
	errno = 0;
	FILE* file = fopen(path, "r");
	if (!file)
		goto error;

	struct String data = ReadFile(file);
	fclose(file);

	errno = 0;
	file = fopen(newPath, "w");
	if (!file)
		goto error;

	if (data.length > 0)
		if (!fwrite(data.ptr, data.length, 1, file))
			goto error;
	free(data.ptr);

	fclose(file);
	return;

error:
	fprintf(stderr, "Failed to write output file: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

static void RestoreOriginalFile(void)
{
	if (!currentFile)
	{
		ASSERT(!currentBackupFile);
		return;
	}
	ASSERT(currentBackupFile);

	CopyFile(currentBackupFile, currentFile);
	remove(currentBackupFile);

	currentFile = NULL;
	currentBackupFile = NULL;
}

static bool MatchSubstring(struct String string, size_t position, struct String substring)
{
	if (string.length == 0 ||
		substring.length == 0 ||
		position + substring.length - 1 >= string.length)
		return false;

	return strncmp(string.ptr + position, substring.ptr, substring.length) == 0;
}

static size_t FindSubstring(struct String string, size_t position, struct String substring)
{
	ASSERT(string.length != 0);
	ASSERT(substring.length != 0);

	for (size_t i = position; i < string.length - (substring.length - 1); ++i)
	{
		if (strncmp(string.ptr + i, substring.ptr, substring.length) == 0)
			return i;
	}

	return SIZE_MAX;
}

static struct String AllocErrorMessageFromLine(struct String line, const char* errorPath, size_t* outEnd)
{
	if (!MatchSubstring(line, 0, COMMENT_TOKEN) ||
		!MatchSubstring(line, COMMENT_TOKEN.length, ERROR_TOKEN))
		return (struct String){.ptr = NULL};

	const size_t errorStringStart = COMMENT_TOKEN.length + ERROR_TOKEN.length;
	const size_t errorStringEnd = FindSubstring(line, errorStringStart, ERROR_TOKEN);
	if (errorStringEnd == SIZE_MAX)
	{
		fprintf(stderr, "Incorrect usage of error token in file: %s\n", errorPath);
		exit(EXIT_FAILURE);
	}

	if (outEnd)
		*outEnd = errorStringEnd + ERROR_TOKEN.length;
	return AllocateString(line.ptr + errorStringStart, errorStringEnd - errorStringStart);
}

static void TestFile(const char* executable, const char* path)
{
	struct String backupPath = AllocFormat("%s%s", path, ".bak");
	CopyFile(path, backupPath.ptr);

	currentFile = path;
	currentBackupFile = backupPath.ptr;

	errno = 0;
	FILE* file = fopen(path, "r+");
	if (!file)
	{
		fprintf(stderr, "Failed to open output file: %s (%s)\n", strerror(errno), path);
		exit(EXIT_FAILURE);
	}

	// initial test which checks to see if it compiles correctly
	// but if there is an error message comment with no code after it at the beginning of the file then it skips this
	// and it will check that instead in the for loop
	struct String first = ReadCurrentLine(file);
	bool run = true;
	if (first.ptr)
	{
		size_t end;
		struct String errorMessage = AllocErrorMessageFromLine(first, path, &end);
		run = !errorMessage.ptr || end != first.length;
		free(errorMessage.ptr);
	}
	if (run)
	{
		struct String expected = AllocateString2("fully compiled to output file");
		RunTest(executable, path, expected, 1);
		free(expected.ptr);
	}
	free(first.ptr);

	struct String string;
	for (size_t lineNumber = 1; (string = ReadCurrentLine(file)).ptr; (NextLine(file), ++lineNumber))
	{
		size_t end;
		struct String errorMessage = AllocErrorMessageFromLine(string, path, &end);
		if (errorMessage.ptr)
		{
			// uncomment
			for (size_t i = 0; i < end; ++i)
				string.ptr[i] = ' ';
			errno = 0;
			if (!fwrite(string.ptr, string.length, 1, file) ||
				fseek(file, -((long)string.length), SEEK_CUR) != 0)
				goto writeError;

			RunTest(executable, path, errorMessage, lineNumber);
			free(errorMessage.ptr);

			// delete line
			memset(string.ptr, ' ', string.length);
			errno = 0;
			if (!fwrite(string.ptr, string.length, 1, file))
				goto writeError;
		}

		free(string.ptr);
	}

	fclose(file);
	RestoreOriginalFile();
	free(backupPath.ptr);
	return;

writeError:
	fprintf(stderr, "Failed to write file: %s (%s)\n", strerror(errno), path);
	exit(EXIT_FAILURE);
}

int main(int argc, const char** argv)
{
	if (atexit(RestoreOriginalFile) != 0)
		ASSERT(0);

	if (argc <= 2)
	{
		fprintf(stderr, "Incorrect number of arguments\n");
		return EXIT_FAILURE;
	}

	for (int i = 2; i < argc; ++i)
		TestFile(argv[1], argv[i]);

	printf("%d tests passed\n", numTestsPassed);
	return EXIT_SUCCESS;
}
