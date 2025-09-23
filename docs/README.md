> [!NOTE]
> These docs arent finished

# Documentation
Since Scythe is built on top of JSFX, it is recommended to be familiar with writing JSFX first. The official JSFX documentation can be found [here](https://www.reaper.fm/sdk/js/js.php).

## File Structure
A Scythe `.scy` file is composed of a series of file-scope top-level statements (all of which are optional). The list of possible statements can be found [here](statements.md#file-scope-statements).\
For example, this file contains a [description statement](), [input statement](), [struct definition](), three [variable declarations](), and a [section statement]().
```c
desc [name: "readme example"];

input speed [
	default: 1,
	min: -2,
	max: 2,
];

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

# Comments
Scythe uses the same comment syntax as C:
```c
// This is a single-line comment
/*
  This is a
  multi-line comment
*/
```
Comments can appear anywhere in `.scy` files.

## Index
- [Statements]()
- [Operators and Expressions]()
- [Type System]()
- [Module System]() - compiling with multiple `.scy` source files
- [Interfacing with JSFX]() - using built in functions / variables
