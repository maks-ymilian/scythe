> [!NOTE]  
> This project is not usable yet

# Scythe
Scythe is a programming language that compiles to [REAPER](https://www.reaper.fm/)'s JSFX.
JSFX makes writing audio effects and instruments for the digital audio workstation [REAPER](https://www.reaper.fm/) fast and immediate.
Scythe uses C-like syntax and aims to make JSFX plugins easier to write and more readable.

Features
- Structs
- Better scoping
- A simple type system, including integer, boolean, pointer, array, and struct types
- Better control flow, including return, break, and continue statements
- For loops
- Variable declaration is enforced before use
- Nested functions
- Namespaces
- Access to all built in JSFX functions
- Full GFX support
- Can be used with libraries written in JSFX
- \+ more

todo code example before and after

# Usage
[REAPER](https://www.reaper.fm/) is required to run JSFX effects.

To compile to a JSFX plugin, run the compiler from the command line:

`scythe <source_file> [output_file]`
- `<source_file>` - Path to your main `.scy` Scythe source file
- `[output_file]` - Path where the generated `.jsfx` file should be saved (optional). If omitted, it will generate the output in the current directory with a default name.

To use JSFX a plugin in REAPER, it must be placed in the `REAPER/Effects/` directory. Once there, it will appear in the FX list and can be found by searching the text in the first `desc:` line of the file, or, if that line is missing, the file name.
