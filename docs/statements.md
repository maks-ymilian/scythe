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

# Expression Statement
An expression statement is composed of an [expression]() followed by a semicolon:
```c
foo = 1 + bar() + baz[69];
```

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
- `@block`
- `@sample`
- `@serialize`
- `@gfx`

Refer to the [JSFX documentation](https://www.reaper.fm/sdk/js/js.php#sec_init) for the specific behaviours of each section.

Sections behave like a void [function](), so [return statements]() are allowed.

## `@init`
`@init` runs on effect startup, sample-rate changes and playback start.
The [built-in variable]() `jsfx.ext_no_init` can be set to `1` to make it run only on effect startup.

## `@gfx`
`@gfx` can include an optional [property list]() that requests the size of the window:
```c
@gfx [width: 500, height: 500]
{
    // code goes here
}
```
`width` or `height` can be omitted to specify that the code does not care what size that dimension is.

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

## Returning
If the function has a non-`void` return type, all code paths must return a value. If the return type is `void`, returning is optional, and values cannot be returned.
```c
any foo(any bar)
{
	return bar;
}

void baz()
{
	return;
}
```

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

Structs can have struct members:
```c
struct Foo
{
	int y;
}

struct Bar
{
	int x;
	Bar bar;
}
```

[Block expressions]() can be used as struct initializers:
```c
struct Foo
{
	int y;
}

struct Bar
{
	int x;
	Bar bar;
}

@init
{
	Bar var1 = Bar {
		.x = 10,
		.bar.y = 20,
	};
	Bar var2 = Bar {};
}
```

For more information, see [Type System]().

# If Statement
If statements are composed of the `if` keyword, a condition expression wrapped in brackets, a statement to run if the condition is true, and optionally the `else` keyword followed by a statement to run if the condition is false.\
The condition expression will attempt to [convert]() to a [`bool`]().
```c
if (foo == 6)
{
	// code goes here
}

if (foo == 7)
{
	// code goes here
}
else
{
	// code goes here
}

if (true)
	bar();

if (foo == 10)
	bar();
else if (foo == 9)
	bar();
else
	bar();
```

# Loop Statements
### While Loop
While loops are composed of the `while` keyword, a condition expression wrapped in brackets, and a statement to run until the condition becomes `false`.\
The condition expression will attempt to [convert]() to a [`bool`]().
```c
while (foo == 6)
{
	// code goes here
}

while (foo == 7)
	bar();
```
Care must be taken to make sure the loop isn't infinite, because if an infinite `while` loop runs in JSFX, REAPER may turn into a slideshow.

### For Loop
For loops are composed of the `for` keyword, brackets containing three semicolon-separated expressions, and a statement to run.\
The first expression can either be an expression or a [variable declaration]().
```c
for (int i = 0; i < 10; ++i)
{
	// code goes here
}
```
All three expressions are optional. If the condition expression is left out, it defaults to `true`, creating an infinite loop:
```c
for (;;)
{
	// infinite loop
}
```

### Loop Control Flow
[`return`](), `break` and `continue` statements work as they do in C-like languages:
```c
for (int i = 0; i < 10; ++i)
{
	if (i == 3)
		continue;
	if (i == 7)
		break;
}
```

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
Input statements are used to define automatable sliders and they compile to [JSFX's slider lines](https://www.reaper.fm/sdk/js/js.php#js_file).\
They are composed of [a modifier list]() (optional), the `input` keyword, an identifier, and a [property list]().\
They can have the following modifiers: [`public`](), [`private`](), [`internal`]()
```basic
input foo [];
input bar [
    name: "display name",
	default: 1000,
	min: 1,
	max: 48000,
	inc: 1,
];
```
The current value can be accessed or set using the `value` member e.g. `foo.value`

The input statement's property list supports the following properties:
| Name | Type | Default Value | Description |
|---|---|---|---|
| `default` | Number | `0` | Initial value |
| `min` | Number | `0` | Minimum value |
| `max` | Number | `10` | Maximum value |
| `inc` | Number | `0` | Step size for the slider UI. This does not affect manually typing values into the slider |
| `name` | String | The input's name | Display name for the slider UI |
| `hidden` | Boolean | `false` | Hides the slider from the UI. This can be used to have an automatable parameter without having it show up in the plugin's UI |
| `shape` | Property list | N/A | Nested property list that controls the slider's shape |

The property list for the `shape` property supports the following properties:
| Name | Type | Default Value | Description |
|---|---|---|---|
| `type` | Enum (`log` or `poly`) | None | The slider will have a logarithmic or polynomial shape depending on if the value of this property is `log` or `poly`. If this property isn't set, the slider is linear |
| `midpoint` | Number | None | If `type` is `log`, this property sets the middle value of the slider |
| `exponent` | Number | `2` | If `type` is `poly`, this property sets the exponent for slider shaping |
| `linear_automation` | Boolean | `false` | If this is set to `true`, the slider shape will not affect automation |

# Description Statement
The description statement corresponds to the [description lines in JSFX](https://www.reaper.fm/sdk/js/js.php#js_file). Only one description statement is allowed throughout all files, so it is recommended to keep it in the main file.\
Description statements are composed of the `desc` keyword followed by a [property list]():
```c
desc [
	description: "variable size circular buffer with adjustable play speed",
	in_pins: [pin: "left", pin: "right"],
	out_pins: [pin: "left", pin: "right"],
	options: [
		max_memory: 16000000,
		no_meter: true,
		gfx: [hz: 60],
	],
];
```

The description statement's property list supports the following properties:
| Name | Type | Default Value | Description |
|---|---|---|---|
| `description` | String | `"scythe effect"` | A short description that will appear in the plugin UI |
| `tags` | String | None | A space delimited list of tags (see [JSFX Documentation for `tags`](https://www.reaper.fm/sdk/js/js.php#js_file)) |
| `in_pins` and `out_pins` | Property list | N/A | Nested property lists containing a list of names for each channel. These names appear in REAPER's plugin pin dialog. (see [JSFX Documentation for `in_pin` and `out_pin` ](https://www.reaper.fm/sdk/js/js.php#js_file)) |
| `options` | Property list | N/A | Nested property list that corresponds to [JSFX's `options:` line](https://www.reaper.fm/sdk/js/js.php#js_file) |

The property list for the `options` property supports the following properties:
| Name | Type | Default Value | Description |
|---|---|---|---|
| `all_keyboard` | Boolean | `false` | Enables the "Send all keyboard input to plug-in" option (see [JSFX Documentation for `want_all_kb`](https://www.reaper.fm/sdk/js/js.php#js_file)) |
| `max_memory` | Number | `8000000` | Requests memory slots (see [JSFX Documentation for `maxmem`](https://www.reaper.fm/sdk/js/js.php#js_file)) |
| `no_meter` | Boolean | `false` | Hides the meters on the sides of the plugin's UI |
| `gfx` | Property list | N/A | Nested property list for gfx-related options |

The property list for the `gfx` property supports the following properties:
| Name | Type | Default Value | Description |
|---|---|---|---|
| `idle_mode` | Enum (`when_closed` or `always`) | None | `when_closed` corresponds to `gfx_idle` in JSFX. `always` corresponds to `gfx_idle_only` in JSFX. (see [JSFX Documentation for `gfx_idle` and `gfx_idle_only`](https://www.reaper.fm/sdk/js/js.php#js_file)) |
| `hz` | Number | `30` | Request [`@gfx` section]() framerate (see [JSFX Documentation for `gfx_hz`](https://www.reaper.fm/sdk/js/js.php#js_file)) |

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

Modifiers can only be used in statements that are currently in [file-scope]().

# Property Lists
Property lists appear in [input statements](), [description statements](), and [@gfx sections](). They are composed of a comma-separated list of properties inside square brackets e.g. `[ property_name: value, property_name: value ]`

Property values can be numbers, strings, booleans, property-specific enums, or other property lists.

This is a property list on an [input statement]():
```c 
input sliderName [
    name: "display name",
	min: 1.5,
	max: 48000,
	shape: [
		type: log,
		midpoint: 1000,
		linear_automation: true,
	],
];
```
