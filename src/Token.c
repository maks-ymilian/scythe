#include "Token.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

const char* GetTokenTypeString(const TokenType tokenType)
{
    switch (tokenType)
    {
    case Token_LeftBracket: return "(";
    case Token_RightBracket: return ")";
    case Token_LeftCurlyBracket: return "{";
    case Token_RightCurlyBracket: return "}";
    case Token_LeftSquareBracket: return "[";
    case Token_RightSquareBracket: return "]";
    case Token_Dot: return ".";
    case Token_Comma: return ",";
    case Token_Semicolon: return ";";
    case Token_Plus: return "+";
    case Token_Minus: return "-";
    case Token_Asterisk: return "*";
    case Token_Slash: return "/";
    case Token_Exclamation: return "!";
    case Token_Equals: return "=";
    case Token_At: return "@";
    case Token_LeftAngleBracket: return "<";
    case Token_RightAngleBracket: return ">";
    case Token_Ampersand: return "&";
    case Token_Pipe: return "|";

    case Token_EqualsEquals: return "==";
    case Token_ExclamationEquals: return "!=";
    case Token_LeftAngleEquals: return "<=";
    case Token_RightAngleEquals: return ">=";
    case Token_AmpersandAmpersand: return "&&";
    case Token_PipePipe: return "||";
    case Token_PlusPlus: return "++";
    case Token_MinusMinus: return "--";
    case Token_PlusEquals: return "+=";
    case Token_MinusEquals: return "-=";
    case Token_AsteriskEquals: return "*=";
    case Token_SlashEquals: return "/=";

    case Token_NumberLiteral: return "NumberLiteral";
    case Token_StringLiteral: return "StringLiteral";
    case Token_Identifier: return "Identifier";
    case Token_True: return "true";
    case Token_False: return "false";

    case Token_If: return "if";
    case Token_Else: return "else";
    case Token_Struct: return "struct";
    case Token_Return: return "return";
    case Token_Import: return "import";
    case Token_Public: return "public";
    case Token_External: return "external";

    case Token_Void: return "void";
    case Token_Int: return "int";
    case Token_Float: return "float";
    case Token_String: return "string";
    case Token_Bool: return "bool";

    case Token_EndOfFile: return "EndOfFile";

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
