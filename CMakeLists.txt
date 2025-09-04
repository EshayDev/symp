cmake_minimum_required(VERSION 3.21)
project(symp VERSION 1.0
			 DESCRIPTION "mach-o symbol patching tool"
			 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_executable(symp
	src/cli.c
	src/macho.c
	src/fileio.c
	src/builtin.c
	src/main.c)
