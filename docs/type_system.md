# Type System
Types can be used in [variable declarations](), [function]() and [block expression]() return types, [structs](), and in the [`sizeof` operator]().

# Types of Types
Types can be separated into two categories:
- [Primitive types]()
- [Aggregate types]()

## Primitive Types
Since all values are doubles in JSFX, the underlying data will always be floating point, regardless of the type you choose. However, some types will restrict what values they will accept to mimic data types, like `int` and `bool`.

You can choose to use these types and their restrictions or avoid them by using the `any` type for everything, but be careful when using `any` with other types, as it lets you store invalid values.

All primitive types can convert to each other, except `bool`, which can't convert to any other type.

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
Converting an `any` to an `int` does not floor the value.

### `bool`
`bool` holds `true` and `false` values.

`bool` cannot convert to or from any other type, except `any`.

### `string` and `char`
`string` and `char` are aliases for `int`. This is because [in JSFX, strings are integer values](https://www.reaper.fm/sdk/js/strings.php#js_strings).

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

Pointer types are identical to `int`, except that they can be dereferenced into their underlying types using the [dereference, arrow, and subscript operators]():
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
Aggregate types are types that can contain one or more members. They can either be [struct types]() or [array types](). An aggregate type can contain aggregate type members, so a struct type can contain struct members. An aggregate type cannot be converted to any other aggregate type.

### Struct Types
[Structs]() allow you to define custom types with multiple members.

### Array Types
Appending `[]` to the end of any non-array and non-pointer type makes it an array type e.g. `Foo[]`

Array types contain two members:
| Name | Type |
|---|---|
| `ptr` | The pointer type corresponding to the array's underlying type<br> e.g. for `int[]` the `ptr` member would be `int*` |
| `length` | `int` |

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
struct Foo {any a; any b; any c}
@init
{
    sizeof(Foo); // 3
    sizeof(Foo[]); // 2
}
```
For more information, see [`sizeof`]()

## `void`
`void` is a special type used only in [function]() and [block expression]() return types. It is used to indicate that the function does not return a value.

# Type Casting
[Object initializers]() can be used to mimic type casting:
```c
float bar = int {5.2}; // bar will be 5
````
