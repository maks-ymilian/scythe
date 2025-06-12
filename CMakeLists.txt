cmake_minimum_required(VERSION 3.10)

project(scythe C)

option(USE_DEBUG "" OFF)
if (USE_DEBUG)
	message(STATUS "Using debug options")
	add_compile_options(
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
		add_compile_options(-O0 -g -fsanitize=address,undefined)
		add_link_options(-fsanitize=address,undefined)
		if (USE_SANITIZERS_EX)
			message(STATUS "Using more sanitizers")
			add_compile_options(-fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
			add_link_options(-fsanitize=address,undefined,leak,pointer-compare,pointer-subtract)
		endif ()
	endif ()
endif ()

add_compile_definitions(_XOPEN_SOURCE=700 _CRT_SECURE_NO_WARNINGS)

set(SOURCES
	"src/Main.c"
	"src/Scanner.c"
	"src/Parser.c"
	"src/SyntaxTree.c"
	"src/Compiler.c"
	"src/PlatformUtils.c"
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
	"src/code-generation/passes/ChainedAssignmentPass.c"
	"src/code-generation/passes/FunctionCallAccessPass.c"
)

option(ASAN_OPTIONS "" OFF)
if (ASAN_OPTIONS)
	message(STATUS "Using ASan options")
	list(APPEND SOURCES "src/ASanOptions.c")
endif ()

add_executable(bin2c "bin2c/bin2c.c")
set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/BuiltIn.c")
set(BUILTIN_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/atomic.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/file.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/gfx.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/jsfx.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/math.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/mem.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/midi.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/pin_mapper.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/slider.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/stack.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/string.scy"
	"${CMAKE_CURRENT_SOURCE_DIR}/scythe/builtin/time.scy"
)
add_custom_command(
	OUTPUT ${GENERATED_FILE}
	COMMENT "Generating ${GENERATED_FILE}"
	DEPENDS ${BUILTIN_FILES}
	COMMAND "$<TARGET_FILE_DIR:bin2c>/bin2c" ${GENERATED_FILE} ${BUILTIN_FILES}
)
list(APPEND SOURCES ${GENERATED_FILE})

add_executable(scythe ${SOURCES})
target_include_directories(scythe PRIVATE "src")
add_dependencies(scythe bin2c)

set_property(TARGET scythe bin2c PROPERTY C_STANDARD 17)
set_property(TARGET scythe bin2c PROPERTY C_EXTENSIONS OFF)
