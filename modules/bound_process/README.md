# BoundProcess module for Godot
This module exposes a new class, `BoundProcess`, to the Godot engine. `BoundProcess` enables developers to spawn a new process and interactively write to its stdin, and read from its stdout and stderr.

## Requirements
This module was built against Godot 4.1.1-stable (bd6af8e0e)

## Usage
Clone this repository into the `modules` folder of a Godot source tree, taking care to ensure the resulting folder is named `bound_process`, then build godot using scons.

1. git clone https://github.com/godotengine/godot
2. cd godot
3. git checkout 4.1.1-stable
4. cd modules
5. git clone https://github.com/turtle-robot/godot-bound-process-module bound_process
6. cd ..
7. scons

## Documentation
Furthur documentation can be found in the Godot editor. Using the "Search Help" tool, search for `BoundProcess`.