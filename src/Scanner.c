#include "Scanner.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

static bool IsIdentifierChar(const char ch)
{
	return isalnum(ch) || ch == '_';
}

static Result ScanIdentifier(void)
{
	const size_t start = pointer;

	if (!isalpha(source[pointer]) && source[pointer] != '_')
		return NOT_FOUND_RESULT;

	while (true)
	{
		if (source[pointer] == '\0')
			break;

		if (!IsIdentifierChar(source[pointer]))
			break;

		pointer++;
	}

	AddTokenSubstring(Token_Identifier, start, pointer);

	return SUCCESS_RESULT;
}

static Result ScanStringLiteral(void)
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

static Result ScanNumberLiteral(void)
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

static Result ScanKeyword(void)
{
	for (TokenType tokenType = 0; tokenType < TokenType_Max; ++tokenType)
	{
		if (tokenType == Token_NumberLiteral ||
			tokenType == Token_StringLiteral ||
			tokenType == Token_Identifier ||
			tokenType == Token_EndOfFile)
			continue;

		const char* string = GetTokenTypeString(tokenType);
		const size_t length = strlen(string);

		if (strlen(source + pointer) < length)
			continue;

		if (IsIdentifierChar(source[pointer]) &&
			IsIdentifierChar(source[pointer + length]))
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

static Result ScanToken(void)
{
	PROPAGATE_FOUND(ScanKeyword());
	PROPAGATE_FOUND(ScanNumberLiteral());
	PROPAGATE_FOUND(ScanIdentifier());
	PROPAGATE_FOUND(ScanStringLiteral());

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
		if (source[pointer] == '\n')
		{
			insideLineComment = false;
			currentLine++;
			pointer++;
			continue;
		}
		else if (source[pointer] == '\n' ||
			source[pointer] == ' ' ||
			source[pointer] == '\t' ||
			source[pointer] == '\r')
		{
			pointer++;
			continue;
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
