cmake_minimum_required(VERSION 3.10)

set(SOURCES
		"src/Main.c"
		"src/Scanner.c"
		"src/Parser.c"
		"src/SyntaxTree.c"
		"src/Compiler.c"
		"src/FileUtils.c"
		"src/StringUtils.c"

		"src/data-structures/Array.c"
		"src/data-structures/Map.c"
		"src/data-structures/MemoryStream.c"

		"src/code-generation/CodeGenerator.c"
		"src/code-generation/passes/ResolverPass.c"
		"src/code-generation/Writer.c"
		"src/code-generation/passes/Common.c"
		"src/code-generation/passes/BlockExpressionPass.c"
		"src/code-generation/passes/GlobalSectionPass.c"
		"src/code-generation/passes/TypeConversionPass.c"
		"src/code-generation/passes/MemberExpansionPass.c"
		"src/code-generation/passes/UniqueNamePass.c"
		"src/code-generation/passes/ControlFlowPass.c"
		"src/code-generation/passes/ReturnTaggingPass.c"
		"src/code-generation/passes/BlockRemoverPass.c"
		"src/code-generation/passes/ForLoopPass.c"
		"src/code-generation/passes/Printer.c"
)

project(scythe C)

add_executable(${PROJECT_NAME} ${SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 23)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)
target_compile_definitions(${PROJECT_NAME} PRIVATE _XOPEN_SOURCE=700)
target_include_directories(${PROJECT_NAME} PRIVATE "src")

option(USE_DEBUG "" OFF)
if (USE_DEBUG)
	target_compile_options(${PROJECT_NAME} PRIVATE
			-Werror -Wall -Wextra -pedantic-errors

			-Wmissing-prototypes
			-Wcast-qual
			-Wsign-conversion

			-Wno-unused-variable
			-Wno-unused-function
			-Wno-unused-parameter
	)

	option(USE_SANITIZERS "" OFF)
	option(USE_SANITIZERS_EX "" OFF)
	if (USE_SANITIZERS OR USE_SANITIZERS_EX)
		target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g -fsanitize=address,undefined)
		target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
		if (USE_SANITIZERS_EX)
			target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
			target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
		endif ()
	endif ()
endif ()
