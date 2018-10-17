cmake_minimum_required( VERSION 3.2.2 )
project( qTriangle CXX )

### Standard
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_VERBOSE_MAKEFILE ON )

# Generate 'compile_commands.json' for clang_complete
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Global includes
include_directories(
	include
)

### Optimizations
if( MSVC )
	add_compile_options( /arch:AVX2 )
	add_compile_options( /W3 )
	add_compile_options( /Gv )
elseif( CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
	add_compile_options( -march=native )
	add_compile_options( -Ofast )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
	# Force colored diagnostic messages in Ninja's output
	if( CMAKE_GENERATOR STREQUAL "Ninja" )
	    add_compile_options( -fdiagnostics-color=always )
	endif()
endif()

## GLM
set( GLM_TEST_ENABLE OFF CACHE BOOL "Build GLM Unit Tests")
add_subdirectory( extern/glm )

### Target
add_library(
	qTriangle
	STATIC
	source/qTriangle/qTriangle.cpp
	source/qTriangle/Util.cpp
)
target_link_libraries(
	qTriangle
	PRIVATE
	glm
)

### Tests
enable_testing()

## Display
add_executable(
	Display
	test/Display.cpp
)
target_link_libraries(
	Display
	PRIVATE
	qTriangle
	glm
)
add_test(
	NAME Display
	COMMAND Display
)

## Benchmark
add_executable(
	Benchmark
	test/Benchmark.cpp
)
target_link_libraries(
	Benchmark
	PRIVATE
	qTriangle
	glm
)
add_test(
	NAME Benchmark
	COMMAND Benchmark
)

## FillShape
add_executable(
	FillShape
	test/FillShape.cpp
)
target_link_libraries(
	FillShape
	PRIVATE
	qTriangle
	glm
)
add_test(
	NAME FillShape
	COMMAND FillShape
)
# Link filesystem libs for GCC/Clang
if( CMAKE_COMPILER_IS_GNUCXX )
	target_link_libraries(
		FillShape
		PRIVATE
		stdc++fs
	)
endif()
