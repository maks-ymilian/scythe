cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME scythe)

project(${PROJECT_NAME} C)

set(SOURCES
        "src/Main.c"
        "src/Scanner.c"
        "src/Token.c"
        "src/Parser.c"
        "src/SyntaxTree.c"
        "src/data-structures/Array.c"
        "src/Result.c"
        "src/code-generation/CodeGenerator.c"
        "src/data-structures/Map.c"
        "src/data-structures/MemoryStream.c"
        "src/code-generation/Data.c"
        "src/code-generation/ExpressionGenerator.c"
        "src/code-generation/StatementGenerator.c"
)

add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "src")