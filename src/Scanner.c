#include "Scanner.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char* currentFile;

static const char* source;
static size_t sourceLength;
static size_t pointer;
static int currentLine;

static Array tokens;

static bool IsEOF(size_t offset)
{
	return pointer + offset >= sourceLength;
}

static void AddTokenSubstring(const TokenType type, size_t start, size_t end)
{
	ArrayAdd(&tokens,
		&(Token){
			.type = type,
			.lineNumber = currentLine,
			.text = source + start,
			.textSize = end - start,
		});
}

static void AddToken(const TokenType type)
{
	ArrayAdd(&tokens,
		&(Token){
			.type = type,
			.lineNumber = currentLine,
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
		if (IsEOF(0))
			break;

		if (!IsIdentifierChar(source[pointer]))
			break;

		pointer++;
	}

	AddTokenSubstring(Token_Identifier, start, pointer);

	return SUCCESS_RESULT;
}

static bool AllowedInStringLiteral(unsigned char c)
{
	return (c >= ' ' && c <= '~') || // printable ascii
		   c >= 128; // utf-8 bytes
}

static Result ScanStringLiteral(bool charLiteral)
{
	const size_t start = pointer;

	const char delimiter = charLiteral ? '\'' : '"';

	if (source[pointer] != delimiter)
		return NOT_FOUND_RESULT;

	bool escape = false;
	++pointer;
	while (true)
	{
		if (IsEOF(0))
			return ERROR_RESULT(charLiteral ? "Char literal is never closed" : "String literal is never closed", currentLine, currentFile);

		if (!AllowedInStringLiteral((unsigned char)source[pointer]))
			return ERROR_RESULT(charLiteral ? "Invalid character in char literal" : "Invalid character in string literal", currentLine, currentFile);

		if (source[pointer] == delimiter && !escape)
			break;

		bool isEscapeChar = source[pointer] == '\\';
		escape = escape ? false : isEscapeChar;

		++pointer;
	}

	AddTokenSubstring(charLiteral ? Token_CharLiteral : Token_StringLiteral, start + 1, pointer);

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
		if (IsEOF(0))
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
			tokenType == Token_CharLiteral ||
			tokenType == Token_Identifier ||
			tokenType == Token_EndOfFile)
			continue;

		const char* string = GetTokenTypeString(tokenType);
		const size_t length = strlen(string);

		if (IsEOF(length))
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
	PROPAGATE_FOUND(ScanStringLiteral(false));
	PROPAGATE_FOUND(ScanStringLiteral(true));

	return ERROR_RESULT("Unexpected character", currentLine, currentFile);
}

Result Scan(const char* path, const char* sourceCode, size_t sourceCodeLength, Array* outTokens)
{
	currentFile = path;

	tokens = AllocateArray(sizeof(Token));
	source = sourceCode;
	sourceLength = sourceCodeLength;
	currentLine = 1;
	pointer = 0;

	bool insideLineComment = false;
	bool insideMultilineComment = false;

	while (!IsEOF(0))
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

		if (!IsEOF(1))
		{
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
