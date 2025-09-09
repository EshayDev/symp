cmake_minimum_required(VERSION 3.21)
project(symp VERSION 1.0
			 DESCRIPTION "mach-o symbol patching tool"
			 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")

# add_compile_options(-Wall -Wshadow -fsanitize=address,undefined)
# add_link_options(-Wall -Wshadow -fsanitize=address,undefined)

add_executable(symp
	src/cli.c
	src/fileio.c
	src/builtin.c
	src/sym/macho.c
	src/sym/symbol.c
	src/sym/objcmeta.c
	src/sym/resolve.c
	src/main.c)

add_custom_command(
	OUTPUT symp.pkg
	COMMAND mkdir -p root
	COMMAND cp symp root
	COMMAND pkgbuild --version 1.0
					 --identifier com.antibiotics.tools
					 --install-location /usr/local/bin
					 --min-os-version 10.15
					 --compression latest
					 --timestamp
					 --root root
					 symp.pkg
	COMMAND rm -r root
	DEPENDS symp
	VERBATIM)

add_custom_target(pkg DEPENDS symp.pkg)

install(TARGETS symp DESTINATION /usr/local/bin)
