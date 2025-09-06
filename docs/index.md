# Introduction
Since Scythe is built on top of JSFX, it is recommended to be familiar with writing JSFX first.
Read the [official JSFX documentation](https://www.reaper.fm/sdk/js/js.php) alongside this guide.

A Scythe `.scy` file is composed of the following (all of which are optional):
- [Sections](#sections)
- [Global Declarations](#global-declarations)
- [Input Statements](#input-statements)
- [Description Statement](#description-statement)
- [Import Statements](#import-statements)

### todo
- code blocks and what is allowed in them
    - loops, control flow
    - if statements
    - block expressions
    - variable declarations
    - function declarations
- types of literals, number literals
- type system, structs and primitive, array types, pointer types
- comments
- operators and precedence
- [Scythe interoperability with JSFX](todo)
- property list
- sizeof

# Sections
Sections are the entry point for code execution. These correspond directly to [JSFX's sections](https://www.reaper.fm/sdk/js/js.php#sec_init).
Section statements are composed of the section type followed by a [code block](todo):
```
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

Sections behave like a [void function](todo), so [return statements](todo) are allowed.

### `@init`
`@init` runs on effect startup, sample-rate changes and playback start.
The [built-in variable](todo) `jsfx.ext_no_init` can be set to `1` to make it run only on effect startup. For more details, see [Scythe interoperability with JSFX](todo).

### `@gfx`
`@gfx` can include an optional [property list](todo) that requests the size of the window. `width` or `height` can be omitted to specify that the code does not care what size that dimension is.
```
@gfx [width: 500, height: 500]
{
    // code goes here
}
```

# Global Declarations
variables, functions, structs

these will be put in an `@init` section in the compiled output

# Input Statements
these correspond with the sliders in jsfx

[property list](todo)

# Description Statement
the description statement corresponds to the description lines in jsfx

[property list](todo)

# Import Statements
[how to use multiple files](todo)
