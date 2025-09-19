# Statements

# Block-scope Statements
Block-scope statements are statements allowed inside of a [block statement]():
- [Block statement]()
- [Expression statement]()
- [Variable declaration]()
- [Function definition]()
- [If statement]()
- [Loop and control flow statements]()

# File-scope Statements
File-scope statements are statements allowed outside of a [block statement]():
- [Section statement]()
- [Variable declaration]()
- [Function definition]()
- [Struct definition]()
- [Import statement]()
- [Input statement]()
- [Description statement]()
- [Modifier statement]()

In the compiled JSFX, the file-scope functions and variables will be put in an [`@init` section]() at the beginning of the [module]().

# Block Statement
Block statements are found in [section statements]() and [function definitions]() and are denoted with `{}` curly brackets.\
Any variables or functions defined in a block statement are local to that block's scope and cannot be used outside of it.

# Sections
Sections are the entry point for code execution. These correspond directly to [JSFX's sections]().
Section statements are composed of the section type followed by a [ statement]():
```c
@init // or other type of section
{
    // code goes here
}
```
There are 6 section types:
- `@init`
- `@slider`
- `@`
- `@sample`
- `@serialize`
- `@gfx`

Refer to the [JSFX documentation](https://www.reaper.fm/sdk/js/js.php#sec_init) for the specific behaviours of each section.

Sections behave like a [void function](), so [return statements]() are allowed.

## `@init`
`@init` runs on effect startup, sample-rate changes and playback start.
The [built-in variable]() `jsfx.ext_no_init` can be set to `1` to make it run only on effect startup.

## `@gfx`
`@gfx` can include an optional [property list]() that requests the size of the window. `width` or `height` can be omitted to specify that the code does not care what size that dimension is.
```c
@gfx [width: 500, height: 500]
{
    // code goes here
}
```

# Variables
Variable declarations are composed of [a modifier list]() (optional), a [type](), an identifier, and an initializer (optional).\
They can have the following modifiers: [`public`](), [`private`](), [`external`](), [`internal`]()
```c
any foo;
int bar = 1; // with initializer
public bool baz; // with modifier list
```

If an initializer is not included, the variable will implicitly initialize to `0` regardless of type. If the type is an [aggregate type](), then all of its members will initialize to `0`. If the variable has the [`external`]() modifier, initializers are not allowed.

Two variables with the same name cannot be defined in the same scope, however, if a variable with the same name is defined in a child scope, it will shadow the previous variable.\
For example, in this code block, the value of `foo` is `2` and the value of `bar` is `1`.
```c
@init
{
    any num = 1;
    {
        any num = 2;
		any foo = num;
    }
	any bar = num;
}
```

# Functions
Function definitions are composed of [a modifier list]() (optional), a return [type](), an identifier, a parameter list (optional), and [block statement]().\
They can have the following modifiers: [`public`](), [`private`](), [`external`](), [`internal`]()
```c++
void foo()
{
    // code goes here
}

public void bar(any param1, any param2)
{
    // code goes here
}
```
Functions can be called like this: `foo()`, `bar(1, 0.5)`

Functions definitions can be nested since they can be written in both [file-scope]() or in [block statements]().\
Recursive functions are not supported, as functions cannot call themselves in JSFX.

## Function Overloads
If the name of a function conflicts with another in the same scope or in a parent scope, it will call the correct overload if it exists, and fail compilation if it doesn't.\
Overloads are distinguished by parameter count only, and parameter types do not affect overload resolution.
```c
void foo(int a) {} // overload a
void foo(int a, int b, int c) {} // overload b

@init
{
	foo(0); // overload a
	foo(0, 0, 0); // overload b

	void foo(int a, int b) {} // overload c

	foo(0, 0); // overload c
}
```

## External Functions
If the function definition has the [`external`]() modifier,
- it must not have a block statement
- it can be a variadic function (optional)
```solidity
external void foo();
```
For more information, see [todo]().

Variadic functions have an ellipsis at the end of the parameter list:
```solidity
external void foo(...);
external void bar(any baz, ...);
```
This feature was only added to support certain variadic functions in the built-in JSFX library, and can be found in the [built-in Scythe library]().

# Structs
Structs are used to define custom [types]().

Struct definitions are composed of [a modifier list]() (optional), the `struct` keyword, and an identifier followed by a series of variable declarations wrapped in curly brackets.\
They can have the following modifiers: [`public`](), [`private`](), [`internal`]()
```c
struct Foo
{
	any bar;
	any baz;
}
```

Once a struct has been defined, it can be used as a type, and struct members can be set or accessed using the [member access operator]():
```c
@init
{
	Foo var1;
	var1.bar = 1;
	var1.baz = "hello";
}
```

For more information, see [Type System]().

# Import Statement
Import statements are used to compile with multiple `.scy` files.

Import statements are composed of [a modifier list]() (optional), and the `import` keyword followed by a string literal containing a file path.\
They can have the following modifiers: [`public`](), [`private`](), [`internal`]()
```go
import "fileName.scy"
import "folder/fileName.scy"
```
Import statements must come before any other statements in the file (except for [modifier statements]())

For more information, see [todo]().

# Input Statement
They can have the following modifiers: [`public`](), [`private`](), [`internal`]()\
these correspond with the sliders in jsfx
[property list](todo)

# Description Statement
the description statement corresponds to the description lines in jsfx
[property list](todo)

# Modifier Statement
A modifier statement is composed of a [modifier list]() followed by a colon.
```solidity
public external:
```

Any statement affected by modifier lists following a modifier statement will have its modifiers applied to it.
For example these, this code:
```solidity
public: // modifier statement
void publicFunc() {}
any publicVar;
private: // modifier statement
void privateFunc() {}
any privateVar;
```
is equivalent to this code:
```solidity
// same code without modifier statements
public void publicFunc() {}
public any publicVar;
private void privateFunc() {}
private any privateVar;
```
\
Modifiers from modifier statements have lower priority than direct modifiers:
```solidity
public:
private int foo; // will be private
int bar; // will be public
```
\
If a modifier type is left unspecified, it won't be affected by a modifier statement:
```solidity
int foo; // private, internal
external:
int bar; // private, external
public:
int baz; // public, external
```

# Modifier Lists
Modifier lists appear before [variable declarations](), [function definitions](), [struct definitions](), [input statements](), [import statements](), or in [their own statement]().

The two possible types of modifiers are:
- `public` or `private` (see [todo]())
- `external` or `internal` (see [todo]())

Modifier lists are composed of one or a combination of these types in any order e.g. `public`, `private external`

Modifiers can only be used in statements that are in [file-scope]().

# Property Lists
Property lists appear in [input statements](), [description statements](), and [@gfx sections](). They are composed of a comma-separated list of properties inside square brackets e.g. `[ property_name: value, property_name: value ]`

Property values can be numbers, strings, booleans, property-specific enums, or other property lists.

This is a property list on an [input statement]():
```c 
input sliderName [
    description: "display name",
	min: 1.5,
	max: 48000,
	shape: [
		type: log,
		midpoint: 1000,
		linear_automation: true,
	],
];
```
