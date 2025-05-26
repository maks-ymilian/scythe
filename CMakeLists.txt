cmake_minimum_required(VERSION 3.10)

set(SOURCES
	"src/Main.c"
	"src/Scanner.c"
	"src/Parser.c"
	"src/SyntaxTree.c"
	"src/Compiler.c"
	"src/FileUtils.c"
	"src/StringUtils.c"
	"src/Token.c"

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

option(ASAN_OPTIONS "" OFF)
if (ASAN_OPTIONS)
	message(STATUS "Using ASan options")
	list(APPEND SOURCES "src/ASanOptions.c")
endif ()

project(scythe C)

add_executable(${PROJECT_NAME} ${SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)
target_compile_definitions(${PROJECT_NAME} PRIVATE _XOPEN_SOURCE=700 _CRT_SECURE_NO_WARNINGS)
target_include_directories(${PROJECT_NAME} PRIVATE "src")

option(USE_DEBUG "" OFF)
if (USE_DEBUG)
	message(STATUS "Using debug options")
	target_compile_options(${PROJECT_NAME} PRIVATE
		-Werror -Wall -Wextra -pedantic-errors

		-Wmissing-prototypes
		-Wcast-qual
		-Wsign-conversion
		-Wconversion
		-Wincompatible-pointer-types
		-Wint-conversion
		-Wuninitialized
		-Wnarrowing
		-Wvla
		-Wunused-variable
		-Wunused-function
		-Wunused-parameter
	)

	option(USE_SANITIZERS "" OFF)
	option(USE_SANITIZERS_EX "" OFF)
	if (USE_SANITIZERS OR USE_SANITIZERS_EX)
		message(STATUS "Using sanitizers")
		target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g -fsanitize=address,undefined)
		target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
		if (USE_SANITIZERS_EX)
			message(STATUS "Using more sanitizers")
			target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
			target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
		endif ()
	endif ()
endif ()
