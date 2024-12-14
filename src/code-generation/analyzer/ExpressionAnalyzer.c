#include "ExpressionAnalyzer.h"

#include <errno.h>

#include "Analyzer.h"
#include "StringUtils.h"

static bool IsDigitBase(const char c, const int base)
{
    if (base == 10) return isdigit(c);
    if (base == 16) return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (base == 2) return c == '0' || c == '1';
    if (base == 8) return c >= '0' && c <= '7';
    assert(0);
}

static int CountCharacters(const char character, const char* string)
{
    assert(string != NULL);

    int count = 0;
    int i = 0;
    while (string[i] != '\0')
    {
        if (string[i] == character)
            count++;
        i++;
    }
    return count;
}

static Result EvaluateNumberLiteral(const Token token, bool* outInteger, char* out)
{
    assert(token.type == Token_NumberLiteral);
    assert(token.text != NULL);

    const size_t length = strlen(token.text);
    char text[length + 1];
    memcpy(text, token.text, length + 1);

    int base = 10;
    if (length >= 3 && text[0] == '0')
    {
        if (text[1] == 'x')
            base = 16;
        else if (text[1] == 'o')
            base = 8;
        else if (text[1] == 'b')
            base = 2;
    }

    bool integer = true;
    if (base == 10)
    {
        const int dotCount = CountCharacters('.', text);
        assert(dotCount >= 0);
        if (dotCount > 1)
            return ERROR_RESULT("Only one dot is allowed in a number literal", token.lineNumber);

        integer = !dotCount;
    }

    if (!integer && base != 10)
        return ERROR_RESULT("Float literal must be base 10", token.lineNumber);

    const size_t startIndex = base == 10 ? 0 : 2;
    const size_t endIndex = length;

    size_t i = startIndex;
    while (i < endIndex)
    {
        if (text[i] == '.')
        {
            if (integer)
                return ERROR_RESULT("Dots in integer literals are not allowed", token.lineNumber);
        }
        else if (!IsDigitBase(text[i], base))
        {
            const char insert[2] = {text[i], '\0'};
            return ERROR_RESULT(AllocateString1Str("Unexpected digit in number literal \"%s\"", insert), token.lineNumber);
        }

        i++;
    }

    text[endIndex] = '\0';
    if (integer)
    {
        const uint64_t number = strtoull(text + startIndex, NULL, base);
        snprintf(out, length + 1, "%llu", number);
        if (errno == ERANGE)
            return ERROR_RESULT("Invalid integer literal", token.lineNumber);
    }
    else
    {
        float _;
        const int ret = sscanf(text + startIndex, "%f", &_);
        if (ret == 0)
            return ERROR_RESULT("Invalid float literal", token.lineNumber);

        memcpy(out, text + startIndex, endIndex - startIndex + 1);
    }

    *outInteger = integer;

    return SUCCESS_RESULT;
}

static Result AnalyzeLiteralExpression(const LiteralExpr* in, Type* outType)
{
    const Token literal = in->token;
    switch (literal.type)
    {
    case Token_NumberLiteral:
    {
        char number[strlen(literal.text) + 1];
        bool integer;
        HANDLE_ERROR(EvaluateNumberLiteral(literal, &integer, number));
        if (outType) *outType = integer ? GetKnownType("int") : GetKnownType("float");

        return SUCCESS_RESULT;
    }
    case Token_StringLiteral:
    {
        if (outType) *outType = GetKnownType("string");
        return SUCCESS_RESULT;
    }
    case Token_True:
    {
        if (outType) *outType = GetKnownType("bool");
        return SUCCESS_RESULT;
    }
    case Token_False:
    {
        if (outType) *outType = GetKnownType("bool");
        return SUCCESS_RESULT;
    }
    default:
        assert(0);
    }
}

static long CountStructVariables(const Type structType)
{
    const SymbolData* symbol = GetKnownSymbol(structType.name, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData* structData = &symbol->structData;

    long count = 0;
    for (int i = 0; i < structData->members->length; ++i)
    {
        const NodePtr* member = structData->members->array[i];
        if (member->type != Node_VariableDeclaration)
            continue;

        const VarDeclStmt* varDecl = member->ptr;
        Type memberType;
        ASSERT_ERROR(GetTypeFromToken(varDecl->type, &memberType, false));

        if (memberType.metaType == MetaType_Struct)
        {
            count += CountStructVariables(memberType);
            continue;
        }

        count++;
    }
    return count;
}

static Result AnalyzeMemberAccessExpression(const MemberAccessExpr* in, Type* outType)
{
    assert(in->value.type == Node_Literal);
    const LiteralExpr* value = in->value.ptr;

    SymbolData* symbol;
    HANDLE_ERROR(GetSymbol(value->token.text, false, NULL, value->token.lineNumber, &symbol));
    assert(symbol->symbolType == SymbolType_Variable);

    const SymbolData* currentSymbol = symbol;
    const MemberAccessExpr* current = in;
    while (current->next.ptr != NULL)
    {
        if (currentSymbol->variableData.type.metaType != MetaType_Struct)
        {
            assert(current->value.type == Node_Literal);
            return ERROR_RESULT(AllocateString1Str("Cannot use access operator on variable \"%s\"",
                                    ((LiteralExpr*)current->value.ptr)->token.text),
                                ((LiteralExpr*)current->value.ptr)->token.lineNumber);
        }

        assert(current->next.type == Node_MemberAccess);
        current = current->next.ptr;

        assert(current->value.type == Node_Literal);
        const LiteralExpr* memberValue = current->value.ptr;

        const SymbolData* memberSymbol = MapGet(&currentSymbol->variableData.symbolTable, memberValue->token.text);
        if (memberSymbol == NULL)
            return ERROR_RESULT(
                AllocateString2Str("Could not find member \"%s\" in struct \"%s\"",
                    memberValue->token.text, currentSymbol->variableData.type.name),
                memberValue->token.lineNumber);
        assert(memberSymbol->symbolType == SymbolType_Variable);
        currentSymbol = memberSymbol;
    }

    assert(currentSymbol->symbolType == SymbolType_Variable);
    if (outType) *outType = currentSymbol->variableData.type;
    return SUCCESS_RESULT;
}

Result CheckAssignmentCompatibility(const Type left, const Type right, const long lineNumber)
{
    if (left.id == right.id)
        return SUCCESS_RESULT;

    const Type intType = GetKnownType("int");
    const Type floatType = GetKnownType("float");
    const Type boolType = GetKnownType("bool");
    const Type convertableTypes[] = {intType, floatType, boolType};
    const size_t convertableTypesSize = sizeof(convertableTypes) / sizeof(Type);

    for (int i = 0; i < convertableTypesSize; ++i)
    {
        if (left.id != convertableTypes[i].id)
            continue;

        if (left.id == boolType.id)
            continue;

        for (int j = 0; j < convertableTypesSize; ++j)
        {
            if (right.id != convertableTypes[j].id)
                continue;

            return SUCCESS_RESULT;
        }
    }

    return ERROR_RESULT(
        AllocateString2Str("Cannot convert from type \"%s\" to type \"%s\"", right.name, left.name),
        lineNumber);
}

Result AnalyzeExpression(const NodePtr* in, Type* outType, const bool expectingExpression);

static Result UnaryOperatorErrorResult(const Token operator, const Type type)
{
    return ERROR_RESULT_TOKEN(AllocateString1Str("Cannot use operator \"#t\" on type \"%s\"", type.name),
                              operator.lineNumber, operator.type);
}

static Result AnalyzeUnaryExpression(const UnaryExpr* in, Type* outType)
{
    Type type;
    HANDLE_ERROR(AnalyzeExpression(&in->expression, &type, true));

    switch (in->operator.type)
    {
    case Token_Exclamation:
        if (type.id != GetKnownType("bool").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    case Token_Minus:
        if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    case Token_Plus:
        if (type.id != GetKnownType("int").id && type.id != GetKnownType("float").id)
            return UnaryOperatorErrorResult(in->operator, type);
        break;
    default:
        assert(0);
    }

    if (outType) *outType = type;

    return SUCCESS_RESULT;
}

static Result AnalyzeBinaryExpression(const BinaryExpr* in, Type* outType)
{
    Type leftType;
    HANDLE_ERROR(AnalyzeExpression(&in->left, &leftType, true));

    Type rightType;
    HANDLE_ERROR(AnalyzeExpression(&in->right, &rightType, true));

    const Result operatorTypeError = ERROR_RESULT_TOKEN(
        AllocateString2Str("Cannot use operator \"#t\" on types \"%s\" and \"%s\"",
            leftType.name, rightType.name),
        in->operator.lineNumber, in->operator.type);

    switch (in->operator.type)
    {
    case Token_Equals:
    case Token_PlusEquals:
    case Token_MinusEquals:
    case Token_SlashEquals:
    case Token_AsteriskEquals:
        // assignment
    {
        bool isLvalue = false;
        // if (in->left.type == Node_Literal)
        // {
        //     const LiteralExpr* literal = in->left.ptr;
        //     if (literal->token.type == Token_Identifier)
        //         isLvalue = true;
        // }
        // else if (in->left.type == Node_ArrayAccess)
        // {
        isLvalue = true;
        // }
        // todo make this check for rvalue

        if (!isLvalue)
            return ERROR_RESULT("Cannot assign to r-value", in->operator.lineNumber);

        HANDLE_ERROR(CheckAssignmentCompatibility(leftType, rightType, in->operator.lineNumber));

        if (outType) *outType = leftType;

        // fallthrough
    }

    case Token_Plus:
    case Token_Minus:
    case Token_Slash:
    case Token_Asterisk:
        // arithmetic
    {
        const Type intType = GetKnownType("int");
        const Type floatType = GetKnownType("float");
        if ((leftType.id != intType.id &&
             leftType.id != floatType.id) ||
            (rightType.id != intType.id &&
             rightType.id != floatType.id))
            return operatorTypeError;

        if (outType)
        {
            if (leftType.id == floatType.id || rightType.id == floatType.id)
                *outType = floatType;
            else
                *outType = intType;
        }

        break;
    }

    case Token_EqualsEquals:
    case Token_ExclamationEquals:
        // equality
    {
        if (leftType.metaType == MetaType_Struct || rightType.metaType == MetaType_Struct)
            return operatorTypeError;

        if (!CheckAssignmentCompatibility(leftType, rightType, 69420).success)
            return operatorTypeError;

        if (outType) *outType = GetKnownType("bool");
        break;
    }
    case Token_LeftAngleBracket:
    case Token_RightAngleBracket:
    case Token_LeftAngleEquals:
    case Token_RightAngleEquals:
        // number comparison
    {
        const Type intType = GetKnownType("int");
        const Type floatType = GetKnownType("float");
        if ((leftType.id != intType.id &&
             leftType.id != floatType.id) ||
            (rightType.id != intType.id &&
             rightType.id != floatType.id))
            return operatorTypeError;

        if (outType) *outType = GetKnownType("bool");
        break;
    }
    case Token_AmpersandAmpersand:
    case Token_PipePipe:
        // boolean operators
    {
        const Type boolType = GetKnownType("bool");
        if (leftType.id != boolType.id || rightType.id != boolType.id)
            return operatorTypeError;

        if (outType) *outType = GetKnownType("bool");
        break;
    }

    default: assert(0);
    }

    return SUCCESS_RESULT;
}

Result AnalyzeExpression(const NodePtr* in, Type* outType, const bool expectingExpression)
{
    switch (in->type)
    {
    case Node_Binary: return AnalyzeBinaryExpression(in->ptr, outType);
    case Node_Unary: return AnalyzeUnaryExpression(in->ptr, outType);
    case Node_Literal: return AnalyzeLiteralExpression(in->ptr, outType);
    case Node_MemberAccess: return AnalyzeMemberAccessExpression(in->ptr, outType);
    case Node_Null:
    {
        if (expectingExpression)
            assert(0);
        return SUCCESS_RESULT;
    }
    default:
        assert(0);
    }
}
