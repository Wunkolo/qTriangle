cmake_minimum_required( VERSION 3.2.2 )
project( qTriangle )

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
	extern/glm
)

### Optimizations
if( MSVC )
	add_compile_options( /arch:AVX2 )
	add_compile_options( /W3 )
	add_compile_options( /Gv )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -march=native )
	add_compile_options( -Ofast )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Target
add_library(
	qTriangle
	STATIC
	source/qTriangle/qTriangle.cpp
	source/qTriangle/Util.cpp
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
