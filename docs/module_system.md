# Module system
To call code from another Scythe file, the file must be included with an [import statement](statements.md#import-statement):
```c
import "foo.scy"
```
Importing a file creates a module named after the file (without the extension). [Functions](statements.md#functions), [variables](statements.md#variables), [inputs](statements.md#input-statement), and [structs](statements.md#structs) can be accessed from a module with the [member access operator](operators_and_expressions.md#member-access-operator-xy):
```c
import "foo.scy"
@init
{
    foo.func();
    foo.var = 10;
    foo.inputSlider.value = 10;
    foo.Type bar;
}
```

When using multiple files, the compiler combines each module into a single JSFX file.

# `public` and `private`
`public` and `private` are modifiers that change the visibility of a declaration when accessing it through modules. Accessing a `private` declaration in a module results in a compiler error. All declarations are `private` by default.

For more information, see [Modifiers](statements.md#modifier-lists).

For example, consider these two files:

`module1.scy`
```c#
public void func() {}
public int foo;
private int bar;
int baz;
```
`main.scy`
```c
import "module1.scy"

@init
{
    module1.func();
    module1.foo;
}
```
Only `func` and `foo` are available in `module1` as they are `public`.

<br>

Any [user-defined types](type_system.md#struct-types) used in a `public` declaration must also be `public`:
```c#
public struct Foo { int a; }

public:
struct Bar
{
    Foo foo;
}
void Func(Foo foo) {}
Foo foo;
```
If `Foo` were `private`, this code would fail to compile, as `Foo` is used in `public` declarations.

## `public` Import Statements
If an [import statement](statements.md#import-statement) is `public`, it is imported alongside the current module.
`module1.scy`
```c#
public import "module2.scy"
private import "module3.scy"
```
`main.scy`
```c
import "module1.scy" // also imports module2 but not module3
```
Importing `module1` will also import `module2` but not `module3` as that [import statement]() is `private`.

# Built-in Library
Every file implicitly imports the whole [built-in library](../scythe/builtin), which contains [`external`](interfacing_with_jsfx.md) definitions for each variable and function in the built-in JSFX library. The built-in library does not need to be imported manually.
