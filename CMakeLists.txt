cmake_minimum_required(VERSION 3.21)
project(symp VERSION 1.0
			 DESCRIPTION "mach-o symbol patching tool"
			 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

add_executable(symp
	src/cli.c
	src/fileio.c
	src/builtin.c
	src/sym/macho.c
	src/sym/exports.c
	src/sym/resolve.c
	src/main.c)
