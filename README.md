# m0dular-csgo

A CSGO HvH cheat build on top of m0dular framework.

## Cloning and updating

This repo heavily uses git submodules. Clone with --recursive flag. After pulling an update be sure to run this command:
```
git submodule update --init --recursive
```

## Building

The project utilizes meson as the main build system. It is available through pip or the official github releases page.

Boost is a build requirement (for now). On Windows one way is to get VCPKG and install it through it.

##### Windows
Launch a VS developer command prompt, navigate to the project directory and run:
```
meson build --backend vs20<17|19> --buildtype=<release|debug> --cross-file windows_msvc_meson.txt
```
Go to the build directory and run msbuild on the solution file or open it in Visual Studio and build there.

You might want to run `generate_filters.py` file in python in order to generate Visual Studio file filters.

##### Linux/MacOS
Run:
```
meson build --buildtype=<release|debug>
ninja -C build
```

##### Cross compiling for Windows
Clang and lld can be used to compile a fully functional library for windows on a Linux or Mac operating system.

It is required to acquire a set of header and library files from the Visual Studio installation and put it in a specific structure, which looks like this:
- \<msvc_dir\>:
	- include:
		- msvc
		- shared
		- ucrt
		- um
		- winrt
		- clang
		- boost
		- (zlib)
		- (openssl)
	- lib:
		- msvc
		- ucrt
		- um
		- clang
		- (zlib)
		- (openssl)

The windows toolchain files are spread out in "Microsoft Visual Studio" and "Windows Kits" directories inside program files. Clang headers and libraries might be needed, might not. They were taken from clang on windows and may have needed some patches to work on the local compiler. Zlib and openssl libraries are needed to build the console client executable.

Set up the build by running:
```
CC=clang CXX=clang++ WBUILD=<msvc_dir> ./setupbuild.sh windows <release|debug>
```
Compile with `ninja -C build`. The resulting binaries will have "lib" appended to their name, just like the native Linux counterparats would.

## Configuring

There are various options available in meson_options.txt file. After running the initial meson command it is possible to change the options using
```
meson configure build -D<option>=<value>...
```

## Contributing

Contibutions are welcome. In order to maintain the quality of the project, some code guidelines have to be followed. Check CONTRIBUTE.md for details, but to summarize:
- Tabs are used for indentation.
- Files have to have correct line endings. Have autocrlf enabled.
- All variables are named using lowerCamelCase, constants with UPPERCASE_CHARACTERS. All functions and classes are named using UpperCamelCase.
- Pointer and reference signs go on the left side, right next to the type name.
- The curly brackets go on the new line only in function definitions, everywhere else they are to be placed on the same line as the (if/for/while) statement.
- Avoid using magic statics.
- Avoid including unnecessary headers inside the header file. Do that in the source file.
- Try keeping an empty line on a file end.
