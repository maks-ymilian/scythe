
# Scythe
Scythe is a programming language that compiles to [REAPER](https://www.reaper.fm/)'s JSFX.

JSFX is used for writing audio effects in the digital audio workstation [REAPER](https://www.reaper.fm/).
It makes developing audio effects and instruments fast and immediate.

Scythe uses C-like syntax and aims to make writing JSFX plugins easier and more readable by adding features such as:
- Structs
- Better scoping
- Better control flow, including return and switch statements
- For loops
- Simplified memory management
- Enforcing variable declaration before use
- Nested functions
- Namespaces
- \+ more

I wrote this as my first C project to learn how to make a programming language.
Feedback and contributions are welcome!

# Usage
[REAPER](https://www.reaper.fm/) is required to run JSFX effects.

Download the binary and compile your source files using the command: ```scythe <input_path> [output_path]```
- ```<input_path>```: Path to your main ```.scythe``` source file.
- ```[output_path]``` (optional): Path to the output ```.jsfx``` file. Set this to be inside REAPER's JSFX folder ```REAPER/Effects```. If not specified, it will put the file in the current working directory.

Once the JSFX file has been added to the ```REAPER/Effects``` folder, you can refresh the list of effects if needed:
- Go to the ```Add FX``` window
- Right click and select ```Scan for new plugins```

Your compiled plugin will now appear in the list of available effects.

# compile it NOW
just compile it dumbass

# Examples
examples

# Known issues
- the output code can get extremely messy and unreadable, which makes it difficult to directly work with the jsfx code if thats something you need to do
- it may have performance overhead compared to just writing jsfx, i dont know how much yet

# Documentation
lol