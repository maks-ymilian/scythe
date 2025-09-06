# Introduction
Since Scythe is built on top of JSFX, it is recommended to be familiar with writing JSFX first.
Read the [official JSFX documentation](https://www.reaper.fm/sdk/js/js.php) alongside this guide.

A Scythe `.scy` file is composed of the following (all of which are optional):
- [Sections](#sections)
- [Declarations](#declarations)
- [Input Statements](#input-statements)
- [Description Statement](#description-statement)
- [Import Statements](#import-statements)

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

# Declarations
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

### todo
- index
    - list every document
- basics
    - file structure
        - sections
            - code blocks>
            - property lists>
        - declarations
            - functions variables>
            - structs>
        - input statements
            - property lists>
        - desc statement
            - property lists>
        - import statements>
    - types of literals
        - number literal
        - bool literal
        - string / char literal
        - block expressions>
    - operators
        - binary including precedence
        - unary
        - subscript, dereferencing
        - sizeof>
    - comments
    - property lists
- code blocks
    - variables
    - functions
    - if statements
    - loops, control flow
    - block expressions
- type system
    - types of types
        - primitive types
        - struct types
        - array types
        - pointer types
    - struct definition
    - sizeof
- module system
    - import statements
    - declaration modifiers
        - public / private
        - external / internal>
    - built in library>
- scythe interoperability with JSFX
    - external / internal
    - built in library
