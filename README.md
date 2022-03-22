# m0dular-csgo (minimal)

Minimal base for educational purposes. Fully featured version is available on the `master` branch, but it is rather complex.

## Disclaimer

This hack was built as a testing ground for various code design techniques and anti-cheat evasion was not the primary consideration. Some systems used are extremely simple to detect, so use at your own risk.

## Cloning and updating

This repo heavily uses git submodules. Clone with --recursive flag. After pulling an update be sure to run this command:
```
git submodule update --init --recursive
```

## Building

The project utilizes meson as the main build system. It is available through pip or the official github releases page.

##### Windows
Launch a VS developer command prompt, navigate to the project directory and run:
```
meson build --backend vs20<17|19|22> --buildtype=<release|debug> --cross-file windows_msvc_meson.txt
```
Go to the build directory and run msbuild on the solution file or open it in Visual Studio and build there.

You might want to run `generate_filters.py` file in python in order to generate Visual Studio file filters.

##### Linux/MacOS
Run:
```
meson build --buildtype=<release|debug>
ninja -C build
```

## Loading

On Linux, use provided `load` and `uload` scripts. On windows, you will need to use a DLL injector.

## Structure

##### Core directory

Contains core initialization and hooking routines specific to the game.

* `core/init.cpp` is the entrypoint that initializes everything.

* `core/hooks.cpp` contains various hooks into the game loop. `CreateMove` hook is arguably the most important one, providing access to player command manipulation.

* `core/engine.cpp` interacts with the game engine. Abstracts several details such as team checking, local player information, etc.

##### Features directory

Contains CSGO specific features. They are being dispatched from hooks.

##### SDK directory

Contains all source engine related classes and features generic to typical source engine games.

* `sdk/source_shared` contains definitions shared across all versions of source engine.

* `sdk/source_2007` contains features present in source engine 2007. Depends on `source_shared`.

* `sdk/source_csgo` contains features present in CSGO's engine. Depends on `source_2007`.

* `sdk/features` contains features present in typical source engine games. Some of them require extra glue code in the implementation, thus they have `SOURCE_NO_` definitions to disable them.

###### Framework directory

Contains all game/engine agnostic code to build hacks.

* `sdk/framework/math` contains the m0dular's math library.

* `sdk/framework/utils` contains utilities for performing lower level operations easily.

* `sdk/framework/features` contains engine/game agnostic features that can be implemented so long as m0dular's framework is being utilized at its fullest (not in this minimal base).

* `sdk/framework/interfaces` interfaces that implementor must add to use framework's features (not relevant here).

## Toolchain extras

##### Configuring

There are various options available in meson_options.txt file. After running the initial meson command it is possible to change the options using
```
meson configure build -D<option>=<value>...
```

##### Cross compiling

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


