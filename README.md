# Scythe
Scythe is a programming language that compiles to [REAPER](https://www.reaper.fm/)'s JSFX.
JSFX makes writing audio effects and instruments for the digital audio workstation [REAPER](https://www.reaper.fm/) fast and immediate.
Scythe uses C-like syntax and aims to make JSFX plugins easier to write, read, and maintain.

### Features
- Structs
- Better scoping
- A simple type system, including integer, boolean, pointer, array, and struct types
- Better control flow, including return, break, and continue statements
- For loops
- Variable declaration is enforced before use
- Nested functions
- Namespaces
- Access to all built in JSFX functions
- Full GFX support
- Can be used with libraries written in JSFX
- \+ more

# Example
An audio buffer with adjustable play speed, written in Scythe:
```c
input speed [default_value: 1, min: -2, max: 2];

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
[REAPER](https://www.reaper.fm/) is required to run JSFX effects.

To compile to a JSFX plugin, run the compiler from the command line:

`scythe <source_file> [output_file]`
- `<source_file>` - Path to your main `.scy` Scythe source file
- `[output_file]` - Path where the generated `.jsfx` file should be saved (optional). If omitted, it will generate the output in the current directory with a default name.

To use a JSFX plugin in REAPER, it must be placed in the `REAPER/Effects/` directory. Once there, it will appear in the FX list and can be found by searching the text in the first `desc:` line of the file, or, if that line is missing, the file name.

# Documentation & Examples
Documentation can be found [here](docs/index.md).

Examples can be found [here](scythe/examples/).

# Planned features
- Support for JSFX's named strings
- Enums - with support for JSFX's enum sliders
- Ternary operator
