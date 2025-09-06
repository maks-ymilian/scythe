# Basics
## Scythe File Structure
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

In the compiled JSFX, the file-scope functions and variables will be put in an `@init` section at the beginning.

## Literals
### Number Literal
### Bool Literal
### String / Char Literal
### Block Expression

## Operators
### Binary operators
Binary operators take two arguments e.g. `x + y`. The following table is ordered from highest precedence to lowest. If multiple operators are on the same line, it means they have equal precedence.
| Operator | Name or category |
| - | - |
| `^` | Exponentiation |
| `%` | Modulo |
| `<<` `>>` | Left shift, right shift |
| `*` `/` | Multiplication, division |
| `+` `-` | Addition, subtraction |
| `\|` | Bitwise OR |
| `&` | Bitwise AND |
| `~` | XOR |
| `==` `!=` `<` `>` `<=` `>=` | Equality and relational operators |
| `\|\|` `&&` | Boolean OR, Boolean AND |
| `=` `+=` `-=` `*=` `/=` `%=` `^=` `&=` `\|=` `~=`  | Assignment operators |

The following unordered table lists the input and output types for each binary operator. All inputs will attempt to convert to the operator's input type (see [Type Conversion](type_system.md/#type-conversion)).
| Operator | Name or category | Input type | Output type |
| - | - | - | - |
| `==` `!=` | Equality operators | Any type | bool |
| `\|\|` `&&` | Boolean OR, Boolean AND | bool | bool |
| `<` `>` `<=` `>=` | Relational operators | float | bool |
| `%` `<<` `>>` `\|` `&` `~` | Integer arithmetic | int | int |
| `+` `-` `*` | Addition, subtraction, multiplication | float | float |
| `/` `^` | Division, exponentiation | float | float (or int if both inputs are int) |
| `=` | Assignment operator | Any type | Any type |

### Unary operators
### Subscript
### `sizeof`

## Comments
Scythe uses the same comment syntax as C:
```
// This is a single-line comment
/*
  This is a
  multi-line comment
*/
```
Comments can appear anywhere in `.scy` files.
