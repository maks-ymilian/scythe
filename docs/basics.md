# Basics
## File Structure
A Scythe `.scy` file is composed of the following (all of which are optional):
- [Code Sections](language_constructs.md/#sections)
- Global Declarations
- [Input Statements](language_constructs.md/#input-and-description-statements)
- [Description Statements](language_constructs.md/#input-and-description-statements)
- [Import Statements](module_system.md/#import-statements)

### Global Declarations
File-scope declarations include:
- [Variables](language_constructs.md/#variables) - allowed in code blocks
- [Functions](language_constructs.md/#functions) - allowed in code blocks
- [Structs](language_constructs.md/#struct-definition) - only allowed at global scope

In the compiled JSFX, the file-scope functions and variables will be put in an `@init` section at the beginning of the module.

## Literals
### `int` Literal
`int` literals can be written in base 10, hexadecimal, octal, or binary:
```c
123456 // base 10 (decimal)
0x123abc // base 16 (hexadecimal)
0o1234567 // base 8 (octal)
0b101011 // base 2 (binary)
```
The maximum value for an `int` literal is the 64-bit unsigned integer limit.

### `float` Literal
`float` literals must have a decimal point, and they can only be written in base 10: `6.9`. They do not support infinity values, NaN values, or scientific notation.
The maximum value for a `float` literal is the double precision float limit.

### `bool` Literal
`bool` literals are written using the `true` and `false` keywords.
### `string` / `char` Literal
- `string` literals use double quotes: `"hello"`
- `char` literals use single quotes: `'A'`. Multibyte `char` literals are supported: `'multiple characters'`

Only printable ASCII characters and UTF-8 characters are allowed in `string` and `char` literals.

Like in JSFX, in Scythe all `string` and `char` literals are integers (see [Type System]()).
Other behaviours, such as escaping, are also identical to JSFX (see [JSFX Documentation]())

## Operators and Expressions
The following table is ordered from highest precedence to lowest. If multiple operators are on the same line, it means they have equal precedence.
| Operator or expression | Name or category | Precedence |
| - | - | - |
| `(x)` [`sizeof x`]() [`x[y]`]() [`x.y`]() [`x->y`]() [literals](), [block expressions]() | Primary expressions| Highest |
| `+x` `-x` `!x` `++x` `--x` [`*x`]() | Prefix unary operators |
| `x++` `x--` | Postfix unary operators |
| `x ^ y` | Exponentiation |
| `x * y` `x / y` `x % y` | Multiplication, division, modulo |
| `x + y` `x - y` | Addition, subtraction |
| `x << y` `x >> y` | Left shift, right shift |
| `x < y` `x > y` `x <= y` `x >= y` | Relational operators |
| `x == y` `x != y` | Equality operators|
| `x & y` | Bitwise AND |
| `x ~ y` | XOR |
| `x \| y` | Bitwise OR |
| `x && y` | Boolean AND |
| `x \|\| y` | Boolean OR |
| `x = y` `x += y` `x -= y` `x *= y` `x /= y` `x %= y` `x ^= y` `x &= y` `x \|= y` `x ~= y`  | Assignment operators | Lowest |

### Operator Type Conversion
The following table lists the input and output types for each operator. All inputs will attempt to convert to the operator's input type (see [Type System](type_system.md)).
| Operator | Input type | Output type |
| - | - | - |
| `x % y` `x << y` `x >> y` `x \| y` `x & y` `x ~ y` | `int` | `int` |
| `x + y` `x - y` `x * y` `+x` `-x` | `float` | `float` |
| `x / y` `x ^ y` | `float` | `float` (or `int` if both inputs are `int`) |
| `x < y` `x > y` `x <= y` `x >= y` | `float` | `bool` |
| `x \|\| y` `x && y` `!x` | `bool` | `bool` |
| `x == y` `x != y` | Compatible types | `bool` |
| `x = y` | Compatible types | Type of `x` |
| `++x` `--x` `x++` `x--` | `int` or `float` | Type of `x` |
| [`*x`]() [`x[y]`]() | `x` - Any pointer type or `int`<br>`y` - `int` | Underlying type of `x` or `int` |
| [`sizeof x`]() | Type or expression | `int` |

### Member Access Expressions
### Subscript
### `sizeof`
### Block Expressions

## Comments
Scythe uses the same comment syntax as C:
```c
// This is a single-line comment
/*
  This is a
  multi-line comment
*/
```
Comments can appear anywhere in `.scy` files.
