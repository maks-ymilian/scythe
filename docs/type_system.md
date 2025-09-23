# Type System
Types can be used in [variable declarations](), [function]() and [block expression]() return types, [structs](), and in the [`sizeof` operator]().

# Types of Types
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
Appending `*` to the end of any non-array and non-pointer type makes it a pointer type.
Pointer types behave exactly the same as `int`, except that they are able to be dereferenced into their underlying types.

Double pointer types are not supported, however this is a planned feature.

## Aggregate Types
Aggregate types are types that can contain one or more members. They can either be [struct types]() or [array types]().

### Struct Types
[structs]() allow you to define custom types with multiple members.

### Array Types
Appending `[]` to the end of any non-array and non-pointer type makes it an array type.
syntax sugar

## `void`
`void` is a special type used in [function]() and [block expression]() return types. It is used to indicate that the function does not return a value.

# Type Casting

# sizeof
