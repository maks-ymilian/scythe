# Operators and Expressions

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
`bool` literals are written using the `true` and `false` keywords. The value of `true` is `1` and the value of `false` is `0`.

### `string` / `char` Literal
- `string` literals use double quotes: `"hello"`
- `char` literals use single quotes: `'A'`. Multibyte `char` literals are supported: `'multiple characters'`

Only printable ASCII characters and UTF-8 characters are allowed in `string` and `char` literals.

Like in JSFX, in Scythe all `string` and `char` literals are integers (see [Type System]()).
Other behaviours, such as escaping, are also identical to JSFX (see [JSFX Documentation]())

## Operator Precedence
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

## Operator Type Conversion
The following table lists the input and output types for each operator. Inputs will attempt to convert to the operator's input type(s) (see [Type System]()).
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

## Member Access operator `x.y`
The member access operator can be used on:
- [Struct objects]() e.g. `structVar.foo` `getStructVar().bar` `structArray[5].baz` etc.
- [Module names]() e.g. `gfx.x` `math.sin()`
- [Input variables]() e.g. `sliderName.value`

## Subscript operator `x[y]`
`x` - Can be any expression of a [primitive type]() or [array type]()\
`y` - Can be any expression of a [primitive type]()

This operator corresponds to [JSFX's subscript operator](). In JSFX, each plugin has a large memory buffer that can be accessed by using the subscript operator. Indexing into this buffer is not done in bytes, but in memory slots, and each memory slot holds a number value. The memory slot being accessed is determined by the sum of `x` and `y`.

Any integer can be used to access memory:
```c
0[0] = 123; // setting slot 0

int foo = 15;
foo[1] = true; // setting slot 16

string* bar = 20;
bar[2] = "hello"; // setting slot 22
```

If `x` is a [pointer]() to or [array]() of an [aggregate type](), the index `y` refers to the element at that position. This means that if `x` is a pointer to struct type, then each increment of `y` advances by the size of the struct, not just one memory slot. Therefore, accessing or setting memory on a pointer to struct type will access or set multiple memory slots at once, in the order the members were defined in.
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
Block expressions consist of a return type followed by a [block statement](). They behave the same as functions.
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
Block expressions can also be used as object initializers:
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

These initializers can be used as [type casts]():
```c
float bar = int {5.2};
```

## `sizeof x` operator
The `sizeof x` operator returns an integer value of the number of members in `x`. `x` can be a type or an expression. If `x` is an expression, it uses the type of the expression.\
If `x` is a [primitive type](), then `sizeof x` will always be `1`.
```c
struct Vector3
{
    float x;
    float y;
    float z;
}
@init
{
    Vector3 v;

    sizeof(Vector3); // 3
    sizeof(v); // 3
    sizeof(int); // 1
}
```
