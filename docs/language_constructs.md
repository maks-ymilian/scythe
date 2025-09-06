# Language Constructs
## Sections
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
The [built-in variable](todo) `jsfx.ext_no_init` can be set to `1` to make it run only on effect startup.

### `@gfx`
`@gfx` can include an optional [property list](todo) that requests the size of the window. `width` or `height` can be omitted to specify that the code does not care what size that dimension is.
```
@gfx [width: 500, height: 500]
{
    // code goes here
}
```

## Code Blocks
- scope
- variables
- functions
- if statements
- loops, control flow
- block expressions
## Struct Definition>
## Import Statement>
## Input and Description Statements
these correspond with the sliders in jsfx
the description statement corresponds to the description lines in jsfx
[property list](todo)

## Property Lists
