#include "Analyzer.h"

#include "ExpressionAnalyzer.h"
#include "StringUtils.h"

static Result AnalyzeVariableDeclaration(const VarDeclStmt* in, const char* prefix, int* outUniqueName);
Result AnalyzeLoopControlStatement(const LoopControlStmt* in);
static Result AnalyzeStatement(const NodePtr* in);

static Result AnalyzeReturnStatement(const ReturnStmt* in)
{
    const ScopeNode* functionScope = GetScopeType(ScopeType_Function);
    if (functionScope == NULL)
        return ERROR_RESULT("Return statement must be inside a function", in->returnToken.lineNumber);


    if (GetScopeType(ScopeType_Loop) != NULL)
    {
        const LoopControlStmt stmt = {.type = LoopControl_Break, .lineNumber = in->returnToken.lineNumber};
        ASSERT_ERROR(AnalyzeLoopControlStatement(&stmt));
    }

    const Type returnType = functionScope->functionReturnType;
    const bool isVoid = returnType.id == GetKnownType("void").id;
    if (in->expr.type == Node_Null)
    {
        if (!isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" must return a value", returnType.name),
                in->returnToken.lineNumber);
    }
    else
    {
        if (isVoid)
            return ERROR_RESULT(
                AllocateString1Str("A function with return type \"%s\" cannot return a value", returnType.name),
                in->returnToken.lineNumber);

        Type exprType;
        HANDLE_ERROR(AnalyzeExpression(&in->expr, &exprType, true));
        HANDLE_ERROR(CheckAssignmentCompatibility(returnType, exprType, in->returnToken.lineNumber));
    }


    return SUCCESS_RESULT;
}

static Result AnalyzeStructVariableDeclaration(const VarDeclStmt* in, const Type type, const char* prefix)
{
    const SymbolData* symbol = GetKnownSymbol(in->type.text, true, NULL);
    assert(symbol->symbolType == SymbolType_Struct);
    const StructSymbolData data = symbol->structData;

    PushScope();

    for (int i = 0; i < data.members->length; ++i)
    {
        const NodePtr* node = data.members->array[i];
        switch (node->type)
        {
        case Node_VariableDeclaration:
        {
            if (prefix == NULL)
                prefix = "";
            char newPrefix[strlen(prefix) + strlen(in->identifier.text) + 2];
            if (snprintf(newPrefix, sizeof(newPrefix), "%s%s.", prefix, in->identifier.text) == 0)
                assert(0);
            HANDLE_ERROR(AnalyzeVariableDeclaration(node->ptr, newPrefix, NULL));
            break;
        }
        default: assert(0);
        }
    }

    Map symbolTable;
    PopScope(&symbolTable);
    HANDLE_ERROR(RegisterVariable(in->identifier, type, &symbolTable, NULL, false, false, in->public, NULL));

    if (in->initializer.type != Node_Null)
    {
        BinaryExpr expr = (BinaryExpr)
        {
            (NodePtr){&(LiteralExpr){in->identifier}, Node_Literal},
            (Token){Token_Equals, in->type.lineNumber, NULL},
            in->initializer
        };
        const NodePtr node = (NodePtr){&expr, Node_Binary};
        HANDLE_ERROR(AnalyzeExpression(&node, NULL, true));
    }

    return SUCCESS_RESULT;
}

static Result AnalyzeArrayVariableDeclaration(const VarDeclStmt* in, const Type type, const ScopeNode* scope)
{
    int uniqueName;
    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL, NULL, true, false, in->public, &uniqueName));
    Type lengthType;
    HANDLE_ERROR(AnalyzeExpression(&in->arrayLength, &lengthType, true));
    HANDLE_ERROR(CheckAssignmentCompatibility(GetKnownType("int"), lengthType, in->identifier.lineNumber));

    return SUCCESS_RESULT;
}

static Result AnalyzeExternalVariableDeclaration(const VarDeclStmt* in, const Type type)
{
    if (type.metaType != MetaType_Primitive)
        return ERROR_RESULT("Only primitive types are allowed for external variable declarations", in->type.lineNumber);

    if (in->array)
        return ERROR_RESULT("Arrays are not allowed for external variable declarations", in->type.lineNumber);

    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL, in->externalIdentifier.text, false, true, in->public, NULL));

    return SUCCESS_RESULT;
}

static Result AnalyzeVariableDeclaration(const VarDeclStmt* in, const char* prefix, int* outUniqueName)
{
    const ScopeNode* scope = GetCurrentScope();
    const bool globalScope = scope->parent == NULL;

    if (!globalScope && in->public)
        return ERROR_RESULT_TOKEN("Variables with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_Public);
    if (!globalScope && in->external)
        return ERROR_RESULT_TOKEN("Variables with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_External);

    Type type;
    HANDLE_ERROR(GetTypeFromToken(in->type, &type, false));

    if (in->array) return AnalyzeArrayVariableDeclaration(in, type, scope);
    if (in->external) return AnalyzeExternalVariableDeclaration(in, type);

    if (type.metaType != MetaType_Primitive && in->public && !type.public)
        return ERROR_RESULT(
            AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", type.name),
            in->identifier.lineNumber);

    if (type.metaType == MetaType_Struct)
    {
        HANDLE_ERROR(AnalyzeStructVariableDeclaration(in, type, prefix));
        return SUCCESS_RESULT;
    }

    assert(in->identifier.type == Token_Identifier);
    int uniqueName;
    HANDLE_ERROR(RegisterVariable(in->identifier, type, NULL, NULL, false, false, in->public, &uniqueName));

    NodePtr initializer;
    LiteralExpr defaultLiteral;
    if (in->initializer.type == Node_Null)
    {
        if (type.id == GetKnownType("int").id || type.id == GetKnownType("float").id)
            defaultLiteral = (LiteralExpr){(Token){Token_NumberLiteral, in->identifier.lineNumber, "0"}};
        else if (type.id == GetKnownType("bool").id)
            defaultLiteral = (LiteralExpr){(Token){Token_False, in->identifier.lineNumber, NULL}};
        else
            return ERROR_RESULT(AllocateString1Str("Variable of type \"%s\" must be initialized", type.name),
                                in->identifier.lineNumber);

        initializer = (NodePtr){&defaultLiteral, Node_Literal};
    }
    else
        initializer = in->initializer;

    Type initializerType;
    HANDLE_ERROR(AnalyzeExpression(&initializer, &initializerType, true));
    HANDLE_ERROR(CheckAssignmentCompatibility(type, initializerType, in->identifier.lineNumber));

    if (outUniqueName != NULL) *outUniqueName = uniqueName;
    return SUCCESS_RESULT;
}

static bool ControlPathsReturn(const NodePtr node, const bool allPathsMustReturn)
{
    switch (node.type)
    {
    case Node_Return:
        return true;
    case Node_If:
    {
        const IfStmt* ifStmt = node.ptr;
        const bool trueReturns = ControlPathsReturn(ifStmt->trueStmt, allPathsMustReturn);
        const bool falseReturns = ControlPathsReturn(ifStmt->falseStmt, allPathsMustReturn);

        if (allPathsMustReturn)
            return trueReturns && falseReturns;
        return trueReturns || falseReturns;
    }
    case Node_Block:
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
    case Node_While:
    {
        return !allPathsMustReturn;
    }
    case Node_LoopControl:
    case Node_ExpressionStatement:
    case Node_VariableDeclaration:
    case Node_FunctionDeclaration:
    case Node_Null:
        return false;
    default: assert(0);
    }
}

static Result AnalyzeFunctionBlock(const BlockStmt* in)
{
    long returnIfStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(AnalyzeStatement(node));

        if (ControlPathsReturn(*node, false))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            returnIfStatementCount++;
        }
    }

    return SUCCESS_RESULT;
}

static Result ParseParameterArray(const FuncDeclStmt* in, Array* outArray)
{
    Array params = AllocateArray(sizeof(FunctionParameter));
    bool hasOptionalParams = false;
    for (int i = 0; i < in->parameters.length; ++i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == Node_VariableDeclaration);
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

        if (varDecl->initializer.ptr != NULL)
        {
            Type initializerType;
            HANDLE_ERROR(AnalyzeExpression(&varDecl->initializer, &initializerType, true));
            HANDLE_ERROR(CheckAssignmentCompatibility(param.type, initializerType, varDecl->identifier.lineNumber));
        }

        if (param.type.metaType != MetaType_Primitive && !param.type.public && in->public)
            return ERROR_RESULT(
                AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", param.type.name),
                varDecl->identifier.lineNumber);
    }

    *outArray = params;
    return SUCCESS_RESULT;
}

static Result AnalyzeExternalFunctionDeclaration(const FuncDeclStmt* in, const Type returnType)
{
    assert(in->block.type == Node_Null);
    assert(in->identifier.type == Token_Identifier);

    if (returnType.metaType != MetaType_Primitive)
        return ERROR_RESULT("External function declarations must return a primitive type", in->type.lineNumber);

    Array params;
    HANDLE_ERROR(ParseParameterArray(in, &params));

    for (int i = 0; i < params.length; ++i)
    {
        const FunctionParameter* funcParam = params.array[i];
        if (funcParam->type.metaType != MetaType_Primitive)
            return ERROR_RESULT("External function declarations can only have parameters of primitive types",
                                in->identifier.lineNumber);
    }

    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params, in->externalIdentifier.text, true, in->public, NULL));

    return SUCCESS_RESULT;
}

static Result AnalyzeFunctionDeclaration(const FuncDeclStmt* in)
{
    const ScopeNode* scope = GetCurrentScope();
    const bool globalScope = scope->parent == NULL;
    if (!globalScope && in->public)
        return ERROR_RESULT_TOKEN("Functions with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_Public);
    if (!globalScope && in->external)
        return ERROR_RESULT_TOKEN("Functions with the \"#t\" modifier are only allowed in global scope",
                                  in->identifier.lineNumber, Token_External);

    Type returnType;
    HANDLE_ERROR(GetTypeFromToken(in->type, &returnType, true));

    if (returnType.metaType != MetaType_Primitive && in->public && !returnType.public)
        return ERROR_RESULT(
            AllocateString1Str("The type \"%s\" is declared private and cannot be used in a public context", returnType.name),
            in->identifier.lineNumber);

    if (in->external)
        return AnalyzeExternalFunctionDeclaration(in, returnType);

    PushScope();

    ScopeNode* currentScope = GetCurrentScope();
    currentScope->scopeType = ScopeType_Function;
    currentScope->functionReturnType = returnType;

    assert(in->block.type == Node_Block);

    if (!ControlPathsReturn(in->block, true) && returnType.id != GetKnownType("void").id)
        return ERROR_RESULT("Not all control paths return a value", in->identifier.lineNumber);

    assert(in->identifier.type == Token_Identifier);

    Array params;
    HANDLE_ERROR(ParseParameterArray(in, &params));

    for (int i = in->parameters.length - 1; i >= 0; --i)
    {
        const NodePtr* node = in->parameters.array[i];
        assert(node->type == Node_VariableDeclaration);
        const VarDeclStmt* varDecl = node->ptr;

        HANDLE_ERROR(AnalyzeVariableDeclaration(varDecl, NULL, NULL));
        Type type;
        ASSERT_ERROR(GetTypeFromToken(varDecl->type, &type, false));
    }

    HANDLE_ERROR(AnalyzeFunctionBlock(in->block.ptr));

    PopScope(NULL);

    int uniqueName;
    HANDLE_ERROR(RegisterFunction(in->identifier, returnType, &params, NULL, false, in->public, &uniqueName));

    return SUCCESS_RESULT;
}

static Result AnalyzeStructDeclaration(const StructDeclStmt* in)
{
    HANDLE_ERROR(RegisterStruct(in->identifier, &in->members, in->public, NULL));

    Type type;
    if (GetTypeFromToken(in->identifier, &type, false).success == false)
        assert(0);

    for (int i = 0; i < in->members.length; ++i)
    {
        const NodePtr* node = in->members.array[i];
        if (node->type != Node_VariableDeclaration)
            continue;

        const VarDeclStmt* varDecl = node->ptr;
        Type varDeclType;
        HANDLE_ERROR(GetTypeFromToken(varDecl->type, &varDeclType, false));
        if (varDeclType.id == type.id)
            return ERROR_RESULT(
                AllocateString2Str("Member \"%s\" of type \"%s\" causes a cycle in the struct layout",
                    varDecl->identifier.text, type.name),
                varDecl->identifier.lineNumber);
    }

    return SUCCESS_RESULT;
}

static Result AnalyzeBlockStatement(const BlockStmt* in)
{
    PushScope();

    for (int i = 0; i < in->statements.length; ++i)
        HANDLE_ERROR(AnalyzeStatement(in->statements.array[i]));

    PopScope(NULL);

    return SUCCESS_RESULT;
}

static Result AnalyzeSectionStatement(const SectionStmt* in)
{
    HANDLE_ERROR(AnalyzeBlockStatement(in->block.ptr));
    return SUCCESS_RESULT;
}

static Result AnalyzeIfStatement(const IfStmt* in)
{
    Type exprType;
    HANDLE_ERROR(AnalyzeExpression(&in->expr, &exprType, true));
    const Type boolType = GetKnownType("bool");
    if (exprType.id != boolType.id)
        return ERROR_RESULT(
            AllocateString1Str("Expression inside if statement must be evaluate to a \"%s\"", boolType.name),
            in->ifToken.lineNumber);

    HANDLE_ERROR(AnalyzeStatement(&in->trueStmt));
    if (in->falseStmt.type != Node_Null)
        HANDLE_ERROR(AnalyzeStatement(&in->falseStmt));

    return SUCCESS_RESULT;
}

static bool ControlPathsContainLoopControl(const NodePtr node)
{
    switch (node.type)
    {
    case Node_LoopControl:
    case Node_Return:
        return true;
    case Node_If:
    {
        const IfStmt* ifStmt = node.ptr;
        const bool trueReturns = ControlPathsContainLoopControl(ifStmt->trueStmt);
        const bool falseReturns = ControlPathsContainLoopControl(ifStmt->falseStmt);
        return trueReturns || falseReturns;
    }
    case Node_Block:
    {
        const BlockStmt* block = node.ptr;
        for (int i = 0; i < block->statements.length; ++i)
        {
            const NodePtr* statement = block->statements.array[i];
            if (ControlPathsContainLoopControl(*statement))
                return true;
        }
        return false;
    }
    case Node_While:
    case Node_For:
        return true;
    case Node_ExpressionStatement:
    case Node_VariableDeclaration:
    case Node_FunctionDeclaration:
    case Node_Null:
        return false;
    default: assert(0);
    }
}

static Result AnalyzeWhileBlock(const BlockStmt* in)
{
    const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
    assert(loopScope != NULL);

    long controlStatementCount = 0;
    for (int i = 0; i < in->statements.length; ++i)
    {
        const NodePtr* node = in->statements.array[i];
        HANDLE_ERROR(AnalyzeStatement(node));

        if (ControlPathsContainLoopControl(*node))
        {
            const size_t statementsLeft = in->statements.length - (i + 1);
            if (statementsLeft == 0)
                continue;

            controlStatementCount++;
        }
    }

    return SUCCESS_RESULT;
}

static Result AnalyzeLoopStatement(const NodePtr in)
{
    const WhileStmt* whileStmt = in.ptr;
    const ForStmt* forStmt = in.ptr;

    PushScope();
    ScopeNode* scope = GetCurrentScope();
    scope->scopeType = ScopeType_Loop;

    if (in.type == Node_For && forStmt->initialization.type != Node_Null)
        HANDLE_ERROR(AnalyzeStatement(&forStmt->initialization));

    const NodePtr* condition = NULL;
    const NodePtr* stmt = NULL;
    if (in.type == Node_For)
    {
        condition = &forStmt->condition;
        stmt = &forStmt->stmt;
    }
    else if (in.type == Node_While)
    {
        condition = &whileStmt->expr;
        stmt = &whileStmt->stmt;
    }
    else
        assert(0);

    NodePtr trueExpr;
    if (condition->type == Node_Null)
    {
        int lineNumber;
        if (in.type == Node_For) lineNumber = forStmt->forToken.lineNumber;
        else if (in.type == Node_While) lineNumber = whileStmt->whileToken.lineNumber;
        else
            assert(0);
        trueExpr = (NodePtr)
        {
            .ptr = AllocLiteral((LiteralExpr)
            {
                .token = (Token){.type = Token_True, .lineNumber = lineNumber, .text = NULL}
            }),
            .type = Node_Literal
        };
        condition = &trueExpr;
    }
    Type exprType;
    HANDLE_ERROR(AnalyzeExpression(condition, &exprType, true));

    if (exprType.id != GetKnownType("bool").id)
        return ERROR_RESULT("Expression inside while block must evaluate to a bool type",);

    assert(stmt->type == Node_Block);
    HANDLE_ERROR(AnalyzeWhileBlock(stmt->ptr));

    if (in.type == Node_For)
        HANDLE_ERROR(AnalyzeExpression(&forStmt->increment, NULL, false));

    PopScope(NULL);

    return SUCCESS_RESULT;
}

Result AnalyzeLoopControlStatement(const LoopControlStmt* in)
{
    const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
    if (loopScope == NULL)
    {
        const TokenType token = in->type == LoopControl_Break ? Token_Break : Token_Continue;
        return ERROR_RESULT_TOKEN("Cannot use \"#t\" statement outside of a loop", in->lineNumber, token);
    }

    return SUCCESS_RESULT;
}

static Result AnalyzeStatement(const NodePtr* in)
{
    switch (in->type)
    {
    case Node_Block:
    {
        int functionDepth = -1;
        const ScopeNode* functionScope = GetScopeType(ScopeType_Function);
        if (functionScope != NULL) functionDepth = functionScope->depth;

        int loopDepth = -1;
        const ScopeNode* loopScope = GetScopeType(ScopeType_Loop);
        if (loopScope != NULL) loopDepth = loopScope->depth;

        if (loopDepth > functionDepth)
        {
            if (loopScope != NULL)
                return AnalyzeWhileBlock(in->ptr);
        }
        else
        {
            if (functionScope != NULL)
                return AnalyzeFunctionBlock(in->ptr);
        }

        return AnalyzeBlockStatement(in->ptr);
    }
    case Node_Import: return SUCCESS_RESULT;
    case Node_ExpressionStatement: return AnalyzeExpression(&((ExpressionStmt*)in->ptr)->expr, NULL, false);
    case Node_Return: return AnalyzeReturnStatement(in->ptr);
    case Node_Section: return AnalyzeSectionStatement(in->ptr);
    case Node_VariableDeclaration: return AnalyzeVariableDeclaration(in->ptr, NULL, NULL);
    case Node_FunctionDeclaration: return AnalyzeFunctionDeclaration(in->ptr);
    case Node_StructDeclaration: return AnalyzeStructDeclaration(in->ptr);
    case Node_If: return AnalyzeIfStatement(in->ptr);
    case Node_While: return AnalyzeLoopStatement(*in);
    case Node_For: return AnalyzeLoopStatement(*in);
    case Node_LoopControl: return AnalyzeLoopControlStatement(in->ptr);
    default: assert(0);
    }
}

Result Analyze(const AST* ast)
{
    for (int i = 0; i < ast->statements.length; ++i)
        HANDLE_ERROR(AnalyzeStatement(ast->statements.array[i]));

    return SUCCESS_RESULT;
}
