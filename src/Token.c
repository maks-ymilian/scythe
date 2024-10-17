#include "Token.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

const char* GetTokenTypeString(const TokenType tokenType)
{
    switch (tokenType)
    {
    case LeftBracket: return "(";
    case RightBracket: return ")";
    case LeftCurlyBracket: return "{";
    case RightCurlyBracket: return "}";
    case LeftSquareBracket: return "[";
    case RightSquareBracket: return "]";
    case Dot: return ".";
    case Comma: return ",";
    case Semicolon: return ";";
    case Plus: return "+";
    case Minus: return "-";
    case Asterisk: return "*";
    case Slash: return "/";
    case Exclamation: return "!";
    case Equals: return "=";
    case At: return "@";
    case LeftAngleBracket: return "<";
    case RightAngleBracket: return ">";
    case Ampersand: return "&";
    case Pipe: return "|";

    case EqualsEquals: return "==";
    case ExclamationEquals: return "!=";
    case LeftAngleEquals: return "<=";
    case RightAngleEquals: return ">=";
    case AmpersandAmpersand: return "&&";
    case PipePipe: return "||";
    case PlusPlus: return "++";
    case MinusMinus: return "--";
    case PlusEquals: return "+=";
    case MinusEquals: return "-=";
    case AsteriskEquals: return "*=";
    case SlashEquals: return "/=";

    case NumberLiteral: return "NumberLiteral";
    case StringLiteral: return "StringLiteral";
    case Identifier: return "Identifier";
    case True: return "true";
    case False: return "false";

    case Init: return "init";
    case Sample: return "sample";
    case Serialize: return "serialize";
    case Block: return "block";
    case GFX: return "gfx";
    case Slider: return "slider";

    case If: return "if";
    case Else: return "else";
    case Struct: return "struct";
    case Return: return "return";

    case Void: return "void";
    case Int: return "int";
    case Float: return "float";
    case String: return "string";
    case Bool: return "bool";

    case EndOfFile: return "EndOfFile";

    default: assert(0);
    }
}

Token CopyToken(const Token token)
{
    if (token.text == NULL)
        return token;

    const size_t stringLength = strlen(token.text);
    char* text = malloc(stringLength + 1);
    memcpy((char*)text, token.text, stringLength);
    text[stringLength] = '\0';

    return (Token){token.type, token.lineNumber, text};
}

void PrintTokenArray(const Array* tokens)
{
    printf("\nlength: %"PRIu64", cap: %"PRIu64"\n", tokens->length, tokens->cap);
    for (size_t i = 0; i < tokens->length; i++)
    {
        const Token* current = tokens->array[i];

        printf("%d: %s", (uint32_t)current->lineNumber, GetTokenTypeString(current->type));

        if (current->text != NULL)
            printf(" \"%s\"", current->text);

        printf("\n");
    }
    printf("\n");
}

void FreeTokenArray(const Array* tokens)
{
    for (int i = 0; i < tokens->length; ++i)
    {
        FreeToken(tokens->array[i]);
    }
    FreeArray(tokens);
}

void FreeToken(const Token* token)
{
    if (token->text != NULL)
        free((void*)token->text);
}
