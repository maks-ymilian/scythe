# Operators and Expressions

## Literals
### Number Literal
Number literals can be written as integers or fractional numbers. They always evaluate to a [`float`](type_system.md#float) type, even if they are integer literals.

#### Fractional Numbers
Frational numbers must have a decimal point, and they can only be written in base 10: `6.9`.\
They do not support infinity values, NaN values, or scientific notation.

#### Integers
Integers can be written in base 10, hexadecimal, octal, or binary:
```c
123456 // base 10 (decimal)
0x123abc // base 16 (hexadecimal)
0o1234567 // base 8 (octal)
0b101011 // base 2 (binary)
```

### [`bool`](type_system.md#bool) Literal
[`bool`](type_system.md#bool) literals are written using the `true` and `false` keywords. The value of `true` is `1` and the value of `false` is `0`.

### [`string`](type_system.md#type_system.md#string-and-char) / [`char`](type_system.md#type_system.md#string-and-char) Literal
- [`string`](type_system.md#type_system.md#string-and-char) literals use double quotes: `"hello"`
- [`char`](type_system.md#type_system.md#string-and-char) literals use single quotes: `'A'`. Multibyte [`char`](type_system.md#type_system.md#string-and-char) literals are supported: `'multiple characters'`

Only printable ASCII characters and UTF-8 characters are allowed in [`string`](type_system.md#type_system.md#string-and-char) and [`char`](type_system.md#type_system.md#string-and-char) literals.

Like in JSFX, in Scythe all [`string`](type_system.md#type_system.md#string-and-char) and [`char`](type_system.md#type_system.md#string-and-char) literals are integers (see [Type System](type_system.md)).
Other behaviours, such as escaping, are also identical to JSFX (see [JSFX Documentation](https://www.reaper.fm/sdk/js/strings.php#js_strings))

## Operator Precedence
The following table is ordered from highest precedence to lowest. If multiple operators are on the same line, it means they have equal precedence.
| Operator or expression | Name or category | Precedence |
| - | - | - |
| `(x)` [`sizeof x`](#sizeof-x-operator) [`x[y]`](#subscript-operator-xy) [`x.y`](#member-access-operator-xy) [`x->y`](#arrow-operator-x-y) [literals](#literals), [block expressions](#block-expressions) | Primary expressions| Highest |
| `+x` `-x` `!x` `++x` `--x` [`*x`](#dereference-operator-x) | Prefix unary operators |
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

## Operator Type Conversion
The following table lists the input and output types for each operator. Inputs will attempt to convert to the operator's input type(s) (see [Type System]()).
| Operator | Input type | Output type |
| - | - | - |
| `x % y` `x << y` `x >> y` `x \| y` `x & y` `x ~ y` | [`int`](type_system.md#int) | [`int`](type_system.md#int) |
| `x + y` `x - y` `x * y` `+x` `-x` `x / y` `x ^ y` | [`float`](type_system.md#float) | [`float`](type_system.md#float) |
| `x < y` `x > y` `x <= y` `x >= y` | [`float`](type_system.md#float) | [`bool`](type_system.md#bool) |
| `x \|\| y` `x && y` `!x` | [`bool`](type_system.md#bool) | [`bool`](type_system.md#bool) |
| `x == y` `x != y` | Compatible types | [`bool`](type_system.md#bool) |
| `x = y` | Compatible types | Type of `x` |
| `++x` `--x` `x++` `x--` | [`int`](type_system.md#int) or [`float`](type_system.md#float) | Type of `x` |
| [`*x`](#dereference-operator-x) [`x[y]`](#subscript-operator-xy) | `x` - Any [pointer type](type_system.md#pointer-types) or [`int`](type_system.md#int)<br>`y` - [`int`](type_system.md#int) | Underlying type of `x` or [`int`](type_system.md#int) |
| [`sizeof x`](#sizeof-x-operator) | Type or expression | [`int`](type_system.md#int) |

## Member Access operator `x.y`
The member access operator can be used on:
- [Struct objects](statements.md#structs) e.g. `structVar.foo` `getStructVar().bar` `structArray[5].baz` etc.
- [Module names](module_system.md) e.g. `gfx.x` `math.sin()`
- [Input variables](statements.md#input-statement) e.g. `sliderName.value`

## Subscript operator `x[y]`
`x` - Can be any expression of a [primitive type](type_system.md#primitive-types) or [array type](type_system.md#array-types)\
`y` - Can be any expression of a [primitive type](type_system.md#primitive-types)

This operator corresponds to [JSFX's subscript operator](https://www.reaper.fm/sdk/js/basiccode.php#op_ram).

The subscript operator is used to read or write [memory slots](README.md#memory), and the memory slot being accessed is determined by the sum of `x` and `y`.

Any integer can be used to access memory:
```c
0[0] = 123; // setting slot 0

int foo = 15;
foo[1] = true; // setting slot 16

string* bar = 20;
bar[2] = "hello"; // setting slot 22
```

The subscript operator can also be used directly on an array type. (see [Array Types](type_system.md#array-types))

If `x` is a [pointer](type_system.md#pointer-types) to or [array](type_system.md#array-types) of an [aggregate type](type_system.md#aggregate-types), the index `y` refers to the element at that position. This means that if `x` is a pointer to struct type, then each increment of `y` advances by the size of the struct, not just one [memory slot](README.md#memory). Therefore, accessing or setting memory on a pointer to struct type will access or set multiple [memory slots](README.md#memory) at once, in the order the members were defined in.
```c
struct Foo
{
    any x;
    any y;
    any z;
}

@init
{
    Foo* buf = 0;
    buf[0] = Foo {.x = 1, .y = "hello", .z = true}; // setting slot 0, 1, and 2
    buf[1] = Foo {.x = 'X', .y = 0.5, .z = 6}; // setting slot 3, 4, and 5
}
```

### Dereference operator `*x`
The dereference operator is syntax sugar e.g. `*foo` becomes `foo[0]`.

### Arrow operator `x->y`
The arrow operator is syntax sugar e.g. `foo->bar` becomes `foo[0].bar`.

## Block Expressions
Block expressions consist of a return type followed by a [block](statements.md#block-statement). They behave the same as functions.
```c
ReturnType {
    // code goes here
};
```
```c
@init
{
    int a = int { return 1; }; // same as writing int a = 1;
    functionCall(bool {
        if (a == 1)
            return false;
        else
            return true;
    });
}
```

### Object Initializers and Type Casting
Block expressions can also be used as object initializers. (see [Structs](statements.md#structs))
```c
Foo bar = Foo {
    .baz = 1,
    .qux = 2,
};

int baz = int {5 + 2};
```

If the initializer is left empty, it will create an object with the default value:
```c
Foo bar = Foo {};
int baz = int {};
```

These initializers can be used as [type casts](type_system.md#type-casting):
```c
float bar = int {5.2}; // bar will be 5
```

## `sizeof x` operator
The `sizeof x` operator returns an [`int`](type_system.md#int) value of the number of [memory slots](#subscript-operator-xy) `x` takes up.\
If `x` is a [primitive type](type_system.md#primitive-types), then this will always be `1`.\
If `x` is an [aggregate type](type_system.md#aggregate-types), then this will be the total number of [primitive type](type_system.md#primitive-types) members in `x`.

`x` can be a type or an expression. If `x` is an expression, it uses the type of the expression.

```c
struct Vector3
{
    float x;
    float y;
    float z;
}

struct Foo
{
    any foo;
    Vector3 vector;
}

@init
{
    Vector3 v;

    sizeof(Vector3); // 3
    sizeof(v); // 3
    sizeof(int); // 1
    sizeof(Foo); // 4
}
```
