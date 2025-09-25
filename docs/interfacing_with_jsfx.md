# Interfacing with JSFX
# `external` and `internal`
Interfacing with JSFX functions and variables is done by using the `external` keyword.\
Marking a function or variable as `external` allows you to use an existing JSFX function or variable in Scythe code.\
All declarations are `internal` by default, so marking a declaration as `internal` has no effect.\
For more information, see [Modifiers](statements.md#modifier-lists).

`external` declarations can only use the [`any`](type_system.md#any) type.\
`external` variables cannot have intitializers.\
`external` functions cannot implement code blocks. Instead, they must end with a semicolon.
```solidity
external any externalVar;
external void externalFunc();
```

You can use the `as` keyword in an `external` declaration to give the function or variable a different name in your code than the one it has in JSFX:
```solidity
external any scytheVar as jsfxVar;
external void scytheFunc() as jsfxFunc;

@init
{
    scytheVar = 69;
    scytheFunc();
}
```

## Variadic Functions
`external` functions can be declared as variadic by appending an ellipsis to the parameter list. Variadic functions can be called with an unlimited number of parameters.
```solidity
external void foo(...);
external void bar(any baz, ...);

@init
{
    bar(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
```
Variadic functions were added specifically to support certain functions in the built-in JSFX library. These functions can be found in the [built-in Scythe library](../scythe/builtin).

# Built-in Library
Every function and variable built-in to JSFX is available through [Scythe's built-in library](../scythe/builtin), which contains `external` definitions for each variable and function in the built-in JSFX library. The built-in library is [imported](module_system.md) automatically and does not need to be [imported](module_system.md) manually.
