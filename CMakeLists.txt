cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME scythe)
set(SOURCES
        "src/Main.c"
        "src/Scanner.c"
        "src/Parser.c"
        "src/SyntaxTree.c"
        "src/data-structures/Array.c"
        "src/code-generation/CodeGenerator.c"
        "src/data-structures/Map.c"
        "src/data-structures/MemoryStream.c"
        "src/code-generation/passes/ResolverPass.c"
        "src/Compiler.c"
        "src/FileUtils.c"
        "src/BuiltInImports.c"
        "src/code-generation/Writer.c"
        "src/code-generation/passes/GlobalSectionPass.c"
        "src/code-generation/passes/TypeConversionPass.c"
)

project(${PROJECT_NAME} C)

add_compile_options(
        -Werror -Wall -Wextra -pedantic-errors

        -Wmissing-prototypes
        -Wcast-qual
        -Wsign-conversion

        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-parameter
)

add_compile_options(-fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined)

add_executable(${PROJECT_NAME} ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 23)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PRIVATE "src")