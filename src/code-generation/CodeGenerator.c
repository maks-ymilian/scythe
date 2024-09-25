#include "CodeGenerator.h"

#include "ExpressionGenerator.h"

static Result GenerateExpressionStatement(const ExpressionStmt* in)
{
    Type type;
    const Result result = GenerateExpression(&in->expr, &type, false, false);
    WRITE_LITERAL(";\n");
    return result;
}

static Result GenerateReturnStatement(const ReturnStmt* in)
{
    const ScopeNode* functionScope = GetFunctionScope();
    if (functionScope == NULL)
        return ERROR_RESULT("Return statement must be inside a function", in->returnToken.lineNumber);

    WRITE_LITERAL("(");
    WRITE_LITERAL("__hasReturned = 1;");

    Result result = SUCCESS_RESULT;
    if (in->expr.type != NullNode)
    {
        WRITE_LITERAL("__returnValue =");
        Type exprType;
        result = GenerateExpression(&in->expr, &exprType, false,
                                    functionScope->functionReturnType.id == GetKnownType("int").id);
    }
    WRITE_LITERAL(");\n");

    return result;
}

static Result GenerateStructVariableDeclaration(const VarDeclStmt* in, Type type);

static Result GenerateVariableDeclaration(const VarDeclStmt* in)
{
    Type type;
    HANDLE_ERROR(GetTypeFromToken(in->type, &type, false));

    if (type.metaType == StructType)
        return GenerateStructVariableDeclaration(in, type);

    assert(in->identifier.type == Identifier);
    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL));

    NodePtr initializer;
    LiteralExpr zero = (LiteralExpr){(Token){NumberLiteral, in->identifier.lineNumber, "0"}};
    if (in->initializer.type == NullNode)
    {
        if (type.id == GetKnownType("int").id || type.id == GetKnownType("float").id)
            initializer = (NodePtr){&zero, LiteralExpression};
        else
            return ERROR_RESULT(AllocateString1Str("Variable of type \"%s\" must be initialized", type.name),
                                in->identifier.lineNumber);
    }
    else
        initializer = in->initializer;

    LiteralExpr literal = {in->identifier};
    const Token equals = {Equals, in->identifier.lineNumber, NULL};
    BinaryExpr binary = {(NodePtr){&literal, LiteralExpression}, equals, initializer};
    const ExpressionStmt stmt = {(NodePtr){&binary, BinaryExpression}};
    HANDLE_ERROR(GenerateExpressionStatement(&stmt));

    return SUCCESS_RESULT;
}

static Result GenerateStructVariableDeclaration(const VarDeclStmt* in, const Type type)
{
    const SymbolData* symbol = GetKnownSymbol(in->type.text, in->type.lineNumber);
    assert(symbol->symbolType == StructSymbol);
    const StructSymbolData data = symbol->structData;

    PushScope();

    for (int i = 0; i < data.members->length; ++i)
    {
        const NodePtr* node = data.members->array[i];
        switch (node->type)
        {
            case VariableDeclaration:
                HANDLE_ERROR(GenerateVariableDeclaration(node->ptr));
                break;
            default: assert(0);
        }
    }

    Map symbolTable;
    PopScope(&symbolTable);
    HANDLE_ERROR(RegisterVariable(in->identifier, type, &symbolTable));

    return SUCCESS_RESULT;
}

static Result CheckReturnStatement(const ReturnStmt* returnStmt, const Type returnType)
{
    const size_t pos = GetStreamPosition();

    const bool isVoid = returnType.id == GetKnownType("void").id;
    if (returnStmt->expr.type == NullNode)
    {
        if (!isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" must return a value", returnType.name),
                returnStmt->returnToken.lineNumber);
    }
    else
    {
        if (isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" cannot return a value", returnType.name),
                returnStmt->returnToken.lineNumber);

        Type exprType;
        HANDLE_ERROR(GenerateExpression(&returnStmt->expr, &exprType, true, false));
        HANDLE_ERROR(CheckAssignmentCompatibility(returnType, exprType,
            "Cannot convert type \"%s\" to return type \"%s\"",
            returnStmt->returnToken.lineNumber));
    }

    SetStreamPosition(pos);

    return SUCCESS_RESULT;
}

static Result CheckReturnStatements(const NodePtr node, const Type returnType)
{
    switch (node.type)
    {
        case ReturnStatement:
            HANDLE_ERROR(CheckReturnStatement(node.ptr, returnType));
            return SUCCESS_RESULT;
        case IfStatement:
        {
            const IfStmt* ifStmt = node.ptr;
            HANDLE_ERROR(CheckReturnStatements(ifStmt->trueStmt, returnType));
            HANDLE_ERROR(CheckReturnStatements(ifStmt->falseStmt, returnType));
            return SUCCESS_RESULT;
        }
        case BlockStatement:
        {
            const BlockStmt* block = node.ptr;
            for (int i = 0; i < block->statements.length; ++i)
            {
                const NodePtr* stmt = block->statements.array[i];
                HANDLE_ERROR(CheckReturnStatements(*stmt, returnType));
            }
            return SUCCESS_RESULT;
        }
        case ExpressionStatement:
        case VariableDeclaration:
        case FunctionDeclaration:
        case NullNode:
            return SUCCESS_RESULT;
        default: assert(0);
    }
}

static bool ControlPathsReturn(const NodePtr node, const bool allPathsMustReturn)
{
    switch (node.type)
    {
        case ReturnStatement:
            return true;
        case IfStatement:
        {
            const IfStmt* ifStmt = node.ptr;
            const bool trueReturns = ControlPathsReturn(ifStmt->trueStmt, allPathsMustReturn);
            const bool falseReturns = ControlPathsReturn(ifStmt->falseStmt, allPathsMustReturn);

            if (allPathsMustReturn)
                return trueReturns && falseReturns;
            return trueReturns || falseReturns;
        }
        case BlockStatement:
        {
            const BlockStmt* block = node.ptr;
            for (int i = 0; i < block->statements.length; ++i)
            {
                const NodePtr* statement = block->statements.array[i];
                if (ControlPathsReturn(*statement, allPathsMustReturn))
                    return true;
            }
            return false;
        }
        case ExpressionStatement:
        case VariableDeclaration:
        case FunctionDeclaration:
        case NullNode:
            return false;
        default: assert(0);
    }
}

static Result GenerateStatement(const NodePtr* in);

static Result GenerateFunctionBlock(const BlockStmt* in, const bool topLevel)
{
    WRITE_LITERAL("(0;\n");

    if (topLevel)
    {
        WRITE_LITERAL("__hasReturned = 0;\n");
        WRITE_LITERAL("__returnValue = 0;\n");
    }

    long returnIfStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(GenerateStatement(node));

        if (ControlPathsReturn(*node, false))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            WRITE_LITERAL("(!__hasReturned) ? (\n");
            returnIfStatementCount++;
        }
    }
    for (int i = 0; i < returnIfStatementCount; ++i)
        WRITE_LITERAL(");");

    if (topLevel)
        WRITE_LITERAL("__returnValue;\n");

    WRITE_LITERAL(");\n");

    return SUCCESS_RESULT;
}

static Result GenerateFunctionDeclaration(const FuncDeclStmt* in)
{
    SetCurrentStream(FunctionsStream);
    PushScope();

    Type returnType;
    HANDLE_ERROR(GetTypeFromToken(in->type, &returnType, true));

    ScopeNode* currentScope = GetCurrentScope();
    currentScope->isFunction = true;
    currentScope->functionReturnType = returnType;

    assert(in->block.type == BlockStatement);

    HANDLE_ERROR(CheckReturnStatements(in->block, returnType));
    if (!ControlPathsReturn(in->block, true) && returnType.id != GetKnownType("void").id)
        return ERROR_RESULT("Not all control paths return a value", in->identifier.lineNumber);

    assert(in->identifier.type == Identifier);

    WRITE_LITERAL("function ");
    WRITE_LITERAL("func_");
    WRITE_TEXT(in->identifier.text);
    WRITE_LITERAL("(");

    if (in->parameters.length > 40)
        return ERROR_RESULT("Functions can only have up to 40 parameters", in->identifier.lineNumber);

    Array params = AllocateArray(sizeof(FunctionParameter));
    bool hasOptionalParams = false;
    for (int i = 0; i < in->parameters.length; ++i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == VariableDeclaration);

        const VarDeclStmt* varDecl = node->ptr;

        if (varDecl->initializer.ptr != NULL)
            hasOptionalParams = true;
        else if (hasOptionalParams == true)
            return ERROR_RESULT("Optional parameters must be at end of parameter list", in->identifier.lineNumber);

        FunctionParameter param;
        param.name = varDecl->identifier.text;
        param.defaultValue = varDecl->initializer;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &param.type, false));
        ArrayAdd(&params, &param);

        WRITE_LITERAL("var_");
        WRITE_TEXT(varDecl->identifier.text);
        if (i < in->parameters.length - 1)
            WRITE_LITERAL(",");

        HANDLE_ERROR(RegisterVariable(varDecl->identifier, param.type, NULL));

        if (param.defaultValue.ptr != NULL)
        {
            const size_t pos = GetStreamPosition();
            HANDLE_ERROR(GenerateFunctionParameter(param.type, &param.defaultValue, varDecl->identifier.lineNumber));
            SetStreamPosition(pos);
        }
    }

    WRITE_LITERAL(")\n");
    HANDLE_ERROR(GenerateFunctionBlock(in->block.ptr, true));

    PopScope(NULL);
    SetCurrentStream(MainStream);

    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params));

    return SUCCESS_RESULT;
}

static Result GenerateStructDeclaration(const StructDeclStmt* in)
{
    HANDLE_ERROR(RegisterStruct(in->identifier, &in->members));
    return SUCCESS_RESULT;
}

static Result GenerateBlockStatement(const BlockStmt* in)
{
    PushScope();

    WRITE_LITERAL("(0;\n");
    for (int i = 0; i < in->statements.length; ++i)
        HANDLE_ERROR(GenerateStatement(in->statements.array[i]));
    WRITE_LITERAL(");\n");

    PopScope(NULL);

    return SUCCESS_RESULT;
}

static Result GenerateIfStatement(const IfStmt* in)
{
    Type exprType;
    HANDLE_ERROR(GenerateExpression(&in->expr, &exprType, true, false));
    const Type boolType = GetKnownType("bool");
    if (exprType.id != boolType.id)
        return ERROR_RESULT(
            AllocateString1Str("Expression inside if statement must be evaluate to a \"%s\"", boolType.name),
            in->ifToken.lineNumber);

    WRITE_LITERAL("?(\n");
    HANDLE_ERROR(GenerateStatement(&in->trueStmt));
    WRITE_LITERAL(")");
    if (in->falseStmt.type != NullNode)
    {
        WRITE_LITERAL(":(\n");
        HANDLE_ERROR(GenerateStatement(&in->falseStmt));
        WRITE_LITERAL(")");
    }
    WRITE_LITERAL(";\n");

    return SUCCESS_RESULT;
}

static Result GenerateSectionStatement(const SectionStmt* in)
{
    if (in->type.type != Init &&
        in->type.type != Slider &&
        in->type.type != Block &&
        in->type.type != Sample &&
        in->type.type != Serialize &&
        in->type.type != GFX)
        assert(0);

    WRITE_LITERAL("\n@");
    WRITE_TEXT(GetTokenTypeString(in->type.type));
    WRITE_LITERAL("\n");
    return GenerateBlockStatement(in->block.ptr);
}

static Result GenerateStatement(const NodePtr* in)
{
    switch (in->type)
    {
        case ExpressionStatement:
            return GenerateExpressionStatement(in->ptr);
        case ReturnStatement:
            return GenerateReturnStatement(in->ptr);
        case Section:
            return GenerateSectionStatement(in->ptr);
        case VariableDeclaration:
            return GenerateVariableDeclaration(in->ptr);
        case BlockStatement:
        {
            const ScopeNode* functionScope = GetFunctionScope();
            return functionScope == NULL ? GenerateBlockStatement(in->ptr) : GenerateFunctionBlock(in->ptr, false);
        }
        case FunctionDeclaration:
            return GenerateFunctionDeclaration(in->ptr);
        case StructDeclaration:
            return GenerateStructDeclaration(in->ptr);
        case IfStatement:
            return GenerateIfStatement(in->ptr);
        default:
            assert(0);
    }
}

Result GenerateProgram(const Program* in)
{
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* stmt = in->statements.array[i];
        assert(stmt->type != RootNode);
        HANDLE_ERROR(GenerateStatement(stmt));
    }

    return SUCCESS_RESULT;
}

Result GenerateCode(Program* syntaxTree, uint8_t** outputCode, size_t* length)
{
    InitResources();

    SetCurrentStream(FunctionsStream);
    WRITE_LITERAL("@init\n");
    SetCurrentStream(MainStream);

    const Result result = GenerateProgram(syntaxTree);

    WRITE_LITERAL("\n");
    const Buffer outputBuffer = CombineStreams();

    FreeSyntaxTree((NodePtr){syntaxTree, RootNode});
    FreeResources();

    if (result.hasError)
        return result;

    *outputCode = outputBuffer.buffer;
    *length = outputBuffer.length;

    return SUCCESS_RESULT;
}
