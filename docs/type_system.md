# Type System
Types can be used in [variable declarations](statements.md#variables), [function](statements.md#functions) and [block expression](operators_and_expressions.md#block-expressions) return types, [structs](statements.md#structs), and in the [`sizeof` operator](operators_and_expressions.md#sizeof-x-operator).

# Types of Types
Types can be separated into two categories:
- [Primitive types](#primitive-types)
- [Aggregate types](#aggregate-types)

## Primitive Types
Since all values are doubles in JSFX, the underlying data will always be floating point, regardless of the type you choose. However, some types will restrict what values they will accept to mimic data types, like [`int`](#int) and [`bool`](#bool).

You can choose to use these types and their restrictions or avoid them by using the [`any`](#any) type for everything, but be careful when using [`any`](#any) with other types, as it lets you store invalid values.

All primitive types can convert to each other, except [`bool`](#bool), which can't convert to any other type.

### `float`
`float` holds a floating point value.

### `int`
`int` holds an integer value.

Converting a `float` to an `int` will floor the value at runtime:
```c
@init
{
    int foo = 6.9; // foo will equal 6
}
```
Converting an [`any`](#any) to an `int` does not floor the value.

### `bool`
`bool` holds `true` and `false` values.

`bool` cannot convert to or from any other type, except [`any`](#any).

### `string` and `char`
`string` and `char` are aliases for [`int`](#int). This is because [in JSFX, strings are integer values](https://www.reaper.fm/sdk/js/strings.php#js_strings).

### `any`
`any` can convert to all primitive types and all primitive types can convert to `any`.

`any` does not stop you from storing invalid values in a type:
```c
@init
{
    int foo = any {5.2}; // foo is 5.2 regardless of its type
    bool bar = any {-1}; // bar is -1 regardless of its type
}
```

### Pointer Types
Appending `*` to the end of any type makes it a pointer type.

Pointer types store a number that points to a specific [memory slot](README.md#memory), and they are typically used with the [subscript operator](operators_and_expressions.md#subscript-operator-xy).

Pointer types are identical to [`int`](#int), except for two distinctions:
1. Addition and subtraction work differently:\
When you add or subtract an integer to a pointer, the operation is scaled by the size of the type the pointer refers to.\
If you have `Vec3* p;`, since `sizeof(Vec3)` is `3`, `p + 2` will actually advance the pointer by `6` [memory slots](README.md#memory), not `2`.\
(see [`sizeof`](#sizeof))\
\
This rule also applies to using pointer types with the [subscript operator](operators_and_expressions.md#subscript-operator-xy).
```c
struct Vec3
{
	float x;
	float y;
	float z;
}

@init
{
	Vec3* p = 10;
	Vec3* p1 = p + 2; // p1 points to memory slot 16
}
```

2. They can be dereferenced into their underlying types using the [dereference, arrow, and subscript operators](operators_and_expressions.md#subscript-operator-xy):
```c
struct Foo
{
    Bar bar;
}

@init
{
    Foo* foo = 10;

    Foo foo1 = *foo;
    Foo foo2 = foo[0];
    Bar bar = foo->bar;
}
```

Double pointer types are not supported, however this is a planned feature.

## Aggregate Types
Aggregate types are types that can contain one or more members. They can either be [struct types](#struct-types) or [array types](#array-types). An aggregate type can contain aggregate type members, so a struct type can contain struct members. An aggregate type cannot be converted to any other aggregate type.

### Struct Types
[Structs](statements.md#structs) allow you to define custom types with multiple members.

### Array Types
Appending `[]` to the end of any non-array and non-pointer type makes it an array type e.g. `Foo[]`

Array types are essentially [fat pointers](https://chatgpt.com/?prompt=fat+pointer+struct+in+c) representing a slice in [memory](README.md#memory).

Array types contain two members:
| Name | Type |
|---|---|
| `ptr` | The pointer type corresponding to the array's underlying type<br> e.g. for `int[]` the `ptr` member would be `int*` |
| `length` | [`int`](#int) |

If the [subscript operator `x[y]`](operators_and_expressions.md#subscript-operator-xy) is used on an array type, it will use its `ptr` member as the index:
```c
int[] array;

// these two lines are identical
array[10] = 100;
array.ptr[10] = 100;
```

Array types have a special property where `any[]` can convert to any other array type:
```c
struct Foo
{
	int a;
	bool b;
	float c;
}

int stackPtr = 0;
any[] AllocateArray(any* ptr, int length)
{
	stackPtr += sizeof(any[]);
	return any[] {.ptr = ptr, .length = length};
}

@init
{
	Foo[] array = AllocateArray(100, 10);
}
```

### `sizeof`
`sizeof` can be used to get the number of members in an aggregate type:
```c
struct Foo {any a; any b; any c;}
@init
{
    sizeof(Foo); // 3
    sizeof(Foo[]); // 2
}
```
For more information, see [`sizeof`](operators_and_expressions.md#sizeof-x-operator)

## `void`
`void` is a special type used only in [function](statements.md#functions) and [block expression](operators_and_expressions.md#block-expressions) return types. It is used to indicate that the function does not return a value.

# Type Casting
[Object initializers](operators_and_expressions.md#object-initializers-and-type-casting) can be used to mimic type casting:
```c
float bar = int {5.2}; // bar will be 5
````
