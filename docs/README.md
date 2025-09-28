# Documentation
Since Scythe is built on top of JSFX, it is recommended to be familiar with writing JSFX first. The official JSFX documentation can be found [here](https://www.reaper.fm/sdk/js/js.php).

## Index
- [Statements](statements.md)
- [Operators and Expressions](operators_and_expressions.md)
- [Type System](type_system.md)
- [Module System](module_system.md)
- [Interfacing with JSFX](interfacing_with_jsfx.md)
- [Built-in Library](../scythe/builtin/)

## File Structure
A Scythe `.scy` file is composed of a series of file-scope statements (all of which are optional). The list of possible file-scope statements can be found [here](statements.md#file-scope-statements).\
For example, this file contains a [description statement](statements.md#description-statement), [input statement](statements.md#input-statement), [struct definition](statements.md#structs), three [variable declarations](statements.md#variables), and a [section statement](statements.md#sections).
```c
desc [description: "readme example"];

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

# Memory
Aside from storing data in [variables](statements.md#variables), JSFX plugins can store data in their dedicated memory buffer. (see [JSFX Documentation](https://www.reaper.fm/sdk/js/basiccode.php#op_ram))

Each JSFX plugin has a large memory buffer. This buffer can be accessed in Scythe by using the [subscript](operators_and_expressions.md#subscript-operator-xy) operator. Indexing into this buffer is not done in bytes, but in memory slots, and each memory slot holds a number value (one [primitive type](type_system.md#primitive-types) in Scythe).

[Pointer types](type_system.md#pointer-types) can be used to store memory slots, and [array types](type_system.md#array-types) can be used to represent slices of memory.

JSFX also has built-in functions that can be used to interact with memory: [JSFX Documentation](https://www.reaper.fm/sdk/js/advfunc.php#cat_mem)\
See [Built-in Library](../scythe/builtin) for instructions on how to use these functions.

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
