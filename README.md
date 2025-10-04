# Scythe
Scythe is a programming language that compiles to [REAPER](https://www.reaper.fm/)'s JSFX, the language used for writing audio effects and instruments that you can instantly hear and tweak in real time.

Scythe makes it easier to read and write code, especially for more complicated projects.\
It adds many features to JSFX, including:
- C-like syntax
- Structs
- Block scope
- A simple type system, including integer, boolean, pointer, array, and struct types
- Better control flow, including return, break, and continue statements
- For loops
- Enforcing variable declaration before use
- Nested functions

Scythe provides access to all built-in JSFX functions, meaning it has full GFX support. It can also be used with libraries written in JSFX.

# Example
An audio buffer with adjustable play speed, written in Scythe:
```c
input speed [default: 1, min: -2, max: 2];

struct Sample
{
	float l;
	float r;
}

Sample* buffer;
float write;
float read;

@sample
{
	buffer[write] = Sample {
		.l = jsfx.spl0,
		.r = jsfx.spl1,
	};
	++write;

	jsfx.spl0 = buffer[read].l;
	jsfx.spl1 = buffer[read].r;
	read += speed.value;
}
```
Compiled JSFX output:
```c
desc:readme example
slider1:speed=1<-2,2,0>speed

@init
buffer = 0;
write = 0;
read = 0;

@sample
buffer[(0 | write) * 2 + 0] = spl0;
buffer[(0 | write) * 2 + 1] = spl1;
write += 1;
spl0 = buffer[(0 | read) * 2 + 0];
spl1 = buffer[(0 | read) * 2 + 1];
read += speed;
```

# Usage
[REAPER](https://www.reaper.fm/) is needed to run JSFX effects.

First, [download the executable](https://github.com/maks-ymilian/scythe/releases) or [compile it yourself](#build-instructions).

To compile to a JSFX plugin, open a terminal in the executable's directory and run
```
scythe <source_file> [output_file]
```
where
- `<source_file>` is the path to your main `.scy` Scythe source file
- `[output_file]` (optional) is the path to where the generated `.jsfx` file should be saved. If omitted, it will generate the output in the current directory with a default name.

To use a JSFX plugin in REAPER, it must be placed in the `REAPER/Effects/` directory. Once there, it will appear in the FX list.

# Documentation & Examples
Documentation can be found [here](docs/README.md).\
Examples can be found [here](scythe/examples/).

# Build Instructions
## Windows
Requirements
- CMake
- Visual Studio with "Desktop development with C++"
1. `git clone` or download the repo
2. Open a terminal in the repo and run
```
mkdir build && cd build && cmake .. && cmake --build . --config Release
```
This will generate a `scythe.exe` executable somewhere in the newly created `build` folder, usually in `build\Release`.

## Linux
Requirements
- CMake
- Make
1. `git clone` or download the repo
2. Open a terminal in the repo and run
```
mkdir build && cd build && cmake .. && cmake --build . --config Release
```
This will generate a `scythe` executable in the newly created `build` directory.
