# Built-in Library
This is the built-in library that gets [imported](../../docs/module_system.md) automatically in every Scythe file. It includes definitions for all documented built-in JSFX functions and variables, along with a few undocumented ones.

If you know the name of a built-in JSFX function or variable you want to use, you can use GitHub search to find its Scythe definition in this folder.\
For example, here's the definition for the JSFX `memcpy` function from the [`mem.scy` file](mem.scy):
```c
any cpy(any dest, any source, any length) as memcpy;
```
In Scythe, you would call it like this:
```c
@init
{
    mem.cpy(64, 0, 64);
}
```
It changes from `memcpy` to `mem.cpy` because the function is defined as `cpy` in the `mem.scy` file.\
For more information, see [Module System](../../docs/module_system.md) and [Interfacing with JSFX](../../docs/interfacing_with_jsfx.md).
