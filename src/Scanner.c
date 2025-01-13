#include "Scanner.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "data-structures/Map.h"

static const char* source;
static size_t pointer;
static int currentLine;

static Array tokens;

static void AddTokenSubstring(const TokenType type, const size_t start, const size_t end)
{
	const size_t length = end - start;
	char* text = malloc(length + 1);
	memcpy(text, source + start, length);
	text[length] = '\0';

	ArrayAdd(&tokens,
		&(Token){
			.type = type,
			.lineNumber = currentLine,
			.text = text,
		});
}

static void AddToken(const TokenType type)
{
	ArrayAdd(&tokens,
		&(Token){
			.type = type,
			.lineNumber = currentLine,
			.text = NULL,
		});
}

static Result ScanIdentifier()
{
	const size_t start = pointer;

	if (!isalpha(source[pointer]) && source[pointer] != '_')
		return NOT_FOUND_RESULT;

	while (true)
	{
		if (source[pointer] == '\0')
			break;

		if (!isalnum(source[pointer]) && source[pointer] != '_')
			break;

		pointer++;
	}

	AddTokenSubstring(Token_Identifier, start, pointer);

	return SUCCESS_RESULT;
}

static Result ScanStringLiteral()
{
	const size_t start = pointer;

	if (source[pointer] != '"')
		return NOT_FOUND_RESULT;

	pointer++;
	while (true)
	{
		if (source[pointer] == '\0')
			return ERROR_RESULT("String literal is never closed", currentLine, NULL);

		if (source[pointer] == '"')
			break;

		pointer++;
	}

	AddTokenSubstring(Token_StringLiteral, start + 1, pointer);

	pointer++;
	return SUCCESS_RESULT;
}

static Result ScanNumberLiteral()
{
	const size_t start = pointer;

	if (!isdigit(source[pointer]))
		return NOT_FOUND_RESULT;

	while (true)
	{
		if (source[pointer] == '\0')
			break;

		if (!isalnum(source[pointer]) && source[pointer] != '.')
			break;

		pointer++;
	}

	AddTokenSubstring(Token_NumberLiteral, start, pointer);

	return SUCCESS_RESULT;
}

static Result ScanKeyword()
{
	for (TokenType tokenType = 0; tokenType < TokenType_Max; ++tokenType)
	{
		switch (tokenType)
		{
		case Token_NumberLiteral:
		case Token_StringLiteral:
		case Token_Identifier:
		case Token_EndOfFile:
			continue;
		default:
		}

		const char* string = GetTokenTypeString(tokenType);
		const size_t length = strlen(string);

		if (strlen(source + pointer) < length)
			continue;

		if (memcmp(source + pointer, string, length) == 0)
		{
			AddToken(tokenType);
			pointer += length;
			return SUCCESS_RESULT;
		}
	}

	return NOT_FOUND_RESULT;
}

static Result ScanToken()
{
	Result result = ScanKeyword();
	if (result.type != Result_NotFound) return result;

	result = ScanNumberLiteral();
	if (result.type != Result_NotFound) return result;

	result = ScanIdentifier();
	if (result.type != Result_NotFound) return result;

	result = ScanStringLiteral();
	if (result.type != Result_NotFound) return result;

	return ERROR_RESULT("Unexpected character", currentLine, NULL);
}

Result Scan(const char* const sourceCode, Array* outTokens)
{
	tokens = AllocateArray(sizeof(Token));
	source = sourceCode;
	currentLine = 1;
	pointer = 0;

	bool insideLineComment = false;
	bool insideMultilineComment = false;

	while (source[pointer] != '\0')
	{
		switch (source[pointer])
		{
		case '\n':
			insideLineComment = false;
			currentLine++;
		case ' ':
		case '\t':
		case '\r':
			pointer++;
			continue;

		default:
		}

		if (memcmp(source + pointer, "//", 2) == 0)
		{
			insideLineComment = true;
			pointer += 2;
			continue;
		}
		else if (memcmp(source + pointer, "/*", 2) == 0)
		{
			insideMultilineComment = true;
			pointer += 2;
			continue;
		}
		else if (memcmp(source + pointer, "*/", 2) == 0 && insideMultilineComment)
		{
			insideMultilineComment = false;
			pointer += 2;
			continue;
		}

		if (insideLineComment || insideMultilineComment)
		{
			pointer++;
			continue;
		}

		PROPAGATE_ERROR(ScanToken());
	}

	AddToken(Token_EndOfFile);

	*outTokens = tokens;

	return SUCCESS_RESULT;
}

void FreeTokenArray(const Array* tokens)
{
	for (size_t i = 0; i < tokens->length; ++i)
	{
		const Token* token = tokens->array[i];
		if (token->text != NULL)
			free(token->text);
	}
	FreeArray(tokens);
}
