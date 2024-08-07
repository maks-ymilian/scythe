#include "Scanner.h"

#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

static const char* source;
static long pointer;
static long currentTokenStart;
static long currentLine;

static Array tokens;

static char Advance()
{
    pointer++;
    return source[pointer];
}

static char Peek()
{
    return source[pointer + 1];
}

static bool IsAtEnd()
{
    return source[pointer + 1] == 0;
}

static const char* GetString(const long start, const long end)
{
    int length = (end + 1) - start;
    length += 1;
    char* text = malloc(length);
    assert(text != NULL);
    memcpy(text, source + start, length - 1);
    text[length - 1] = '\0';
    return text;
}

static const char* GetCurrentLexemeString()
{
    return GetString(currentTokenStart, pointer);
}

static void AddNewToken(const TokenType type, const char* text)
{
    const Token token = (Token){type, currentLine, text};
    ArrayAdd(&tokens, &token);
}

typedef struct
{
    const char* text;
    TokenType tokenType;
} KeywordToTokenType;

#define KEYWORD_COUNT 7
static const KeywordToTokenType keywordToTokenType[KEYWORD_COUNT] =
{
    {"init", Init},
    {"slider", Slider},
    {"block", Block},
    {"sample", Sample},
    {"serialize", Serialize},
    {"gfx", GFX},
    {"int", Int},
};

static bool TryGetKeyword(TokenType* outTokenType)
{
    const char* text = GetCurrentLexemeString();

    bool success = false;

    for (size_t i = 0; i < KEYWORD_COUNT; i++)
    {
        if (!strcmp(text, keywordToTokenType[i].text))
        {
            *outTokenType = keywordToTokenType[i].tokenType;
            success = true;
        }
    }

    free((void*)text);

    return success;
}

static void ScanWord()
{
    pointer--;

    while (!IsAtEnd())
    {
        const char character = Advance();

        if (!isalnum(character))
        {
            pointer--;
            break;
        }
    }

    TokenType tokenType;
    if (TryGetKeyword(&tokenType))
        AddNewToken(tokenType, NULL);
    else
        AddNewToken(Identifier, GetCurrentLexemeString());
}

static void ScanStringLiteral()
{
    const long start = pointer + 1;
    while (!IsAtEnd())
    {
        const char character = Advance();

        if (character == '"')
            break;
    }
    const long end = pointer - 1;

    AddNewToken(StringLiteral, GetString(start, end));
}

static void ScanNumberLiteral()
{
    pointer--;

    while (!IsAtEnd())
    {
        const char character = Advance();

        if (!isdigit(character))
        {
            pointer--;
            break;
        }
    }

    AddNewToken(NumberLiteral, GetCurrentLexemeString());
}

typedef struct
{
    char symbol;
    TokenType joinedTokenType;
} SecondSymbol;

static void AddDoubleSymbolToken(const TokenType defaultType, const SecondSymbol* symbols, const size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (Peek() == symbols[i].symbol)
        {
            Advance();
            AddNewToken(symbols[i].joinedTokenType, NULL);
            return;
        }
    }

    AddNewToken(defaultType, NULL);
}

Result Scan(const char* const sourceCode, Array* outTokens)
{
    tokens = AllocateArray(1, sizeof(Token));

    source = sourceCode;
    pointer = -1;
    currentLine = 1;

    bool insideLineComment = false;
    bool insideMultilineComment = false;

    while (!IsAtEnd())
    {
        const char character = Advance();

        if (character == '\n')
        {
            currentLine++;
            insideLineComment = false;
            continue;
        }

        if (character == ' ' ||
            character == '\t' ||
            character == '\r')
            continue;

        currentTokenStart = pointer;

        if (character == '/')
        {
            const char peek = Peek();
            if (peek == '/' && !insideMultilineComment)
            {
                insideLineComment = true;
                continue;
            }
            else if (peek == '*')
            {
                insideMultilineComment = true;
                continue;
            }
        }

        if (character == '*' && Peek() == '/' && insideMultilineComment)
        {
            insideMultilineComment = false;
            Advance();
            continue;
        }

        if (insideLineComment || insideMultilineComment)
            continue;

        switch (character)
        {
            case '(': AddNewToken(LeftBracket, NULL);
                continue;
            case ')': AddNewToken(RightBracket, NULL);
                continue;
            case '{': AddNewToken(LeftCurlyBracket, NULL);
                continue;
            case '}': AddNewToken(RightCurlyBracket, NULL);
                continue;
            case '[': AddNewToken(LeftSquareBracket, NULL);
                continue;
            case ']': AddNewToken(RightSquareBracket, NULL);
                continue;
            case '.': AddNewToken(Dot, NULL);
                continue;
            case ',': AddNewToken(Comma, NULL);
                continue;
            case ';': AddNewToken(Semicolon, NULL);
                continue;
            case '@': AddNewToken(At, NULL);
                continue;

            case '=': AddDoubleSymbolToken(Equals, (SecondSymbol[]){{'=', EqualsEquals}}, 1);
                continue;
            case '!': AddDoubleSymbolToken(Exclamation, (SecondSymbol[]){{'=', ExclamationEquals}}, 1);
                continue;
            case '<': AddDoubleSymbolToken(LeftAngleBracket, (SecondSymbol[]){{'=', LeftAngleEquals}}, 1);
                continue;
            case '>': AddDoubleSymbolToken(RightAngleBracket, (SecondSymbol[]){{'=', RightAngleEquals}}, 1);
                continue;
            case '+': AddDoubleSymbolToken(Plus, (SecondSymbol[]){{'+', PlusPlus}, {'=', PlusEquals}}, 2);
                continue;
            case '-': AddDoubleSymbolToken(Minus, (SecondSymbol[]){{'-', MinusMinus}, {'=', MinusEquals}}, 2);
                continue;
            case '*': AddDoubleSymbolToken(Asterisk, (SecondSymbol[]){{'=', AsteriskEquals}}, 1);
                continue;
            case '/': AddDoubleSymbolToken(Slash, (SecondSymbol[]){{'=', SlashEquals}}, 1);
                continue;
            case '&': AddDoubleSymbolToken(Ampersand, (SecondSymbol[]){{'&', AmpersandAmpersand}}, 1);
                continue;
            case '|': AddDoubleSymbolToken(Pipe, (SecondSymbol[]){{'|', PipePipe}}, 1);
                continue;

            default: break;
        }

        if (isdigit(character)) // number literal
            ScanNumberLiteral();
        else if (isalpha(character)) // identifier / keyword
            ScanWord();
        else if (character == '"') // string literal
            ScanStringLiteral();
        else // error
        {
            FreeArray(&tokens);
            return ERROR_RESULT("Unexpected character", currentLine);
        }
    }
    AddNewToken(EndOfFile, NULL);

    *outTokens = tokens;

    return SUCCESS_RESULT;
}
