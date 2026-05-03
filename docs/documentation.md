# pdvm Documentation

## Overview

`pdvm` is a stack-based VM language. Most commands consume values from the top of the stack and optionally push results back. PDVM supports labels, stack operations, arithmetic, memory access, struct packing, and dynamic library calls.

Core traits:

- Typed scalar values: `u8`, `i32`, `i64`, `f32`, `f64`
- Opaque pointer values: `ptr`
- Packed struct values with C-like alignment rules
- Byte-backed memory storing full typed values
- A C-like preprocessor with `include`, `@define`, and `@if`
- Dynamic loading and foreign calls through `dlopen`, `dlsym`, `dlclose`, and `dlcall`

## File Format

- Commands are line-oriented.
- Leading indentation is allowed but not required.
- A line whose first non-space character is `#` is a comment.
- Commands are not case sensitive.
- Names such as labels, constants, struct names, dynamic library aliases, and preprocessor definitions are case-sensitive.

Every runnable file must eventually define:

```pdvm
label _main
```

Execution begins at `_main`.

## Preprocessor

File execution goes through a preprocessing step before the VM runs the program.

### `include`

```pdvm
include path/to/file.pdvm
```

- Includes another file in source order.
- Relative paths are resolved relative to the including file.
- `include` is a file preprocessor feature, not a REPL command.

### `@define`

```pdvm
@define TERM
```

- Adds a definition name.
- Definitions are presence-based only. There is no macro expansion and no value attached to a definition.

### `@undef`

```pdvm
@undef TERM
```

- Removes a definition if it exists.

### `@if`, `@elif`, `@else`, `@endif`

```pdvm
@if OS_WINDOWS
include raylib_win.pdvm
@elif OS_LINUX
include raylib_linux.pdvm
@elif OS_MACOS
include raylib_macos.pdvm
@else
pushs unsupported platform
error
halt
@endif
```

- `@if NAME` is true when `NAME` is currently defined.
- `@elif NAME` checks another definition if no earlier branch in the same block matched.
- `@else` is the fallback branch.
- `@endif` closes the block.
- Conditionals are processed in source order across included files.

Built-in host definitions:

- `OS_WINDOWS`
- `OS_LINUX`
- `OS_MACOS`

## Data Types

### Scalar value types

- `u8`
- `i32`
- `i64`
- `f32`
- `f64`

### Pointer values

- `ptr`
- Opaque `void *` payloads
- Mainly intended for `dlcall`
- Not directly creatable from source today

### Struct values

- Runtime type: `struct`
- Created with `pack`
- Stored as packed bytes with computed field offsets, size, and alignment

### Strings

Strings are not a dedicated runtime type. A string is represented on the stack as:

```text
u8, u8, ..., u8, i64(length)
```

For example:

```pdvm
pushs hello
```

pushes:

```text
u8('h') u8('e') u8('l') u8('l') u8('o') i64(5)
```

This convention is used by:

- `pushs`
- `inputs`
- `prints`
- `emits`
- `error`
- `dlcall ... str`

## Literals and Constants

### `push`

```pdvm
push 42
push 3.14
push SOME_CONST
```

- Default numeric literal command
- Integers become `i64`
- Decimal literals become `f64`
- Can also push an existing constant

### Explicit typed pushes

```pdvm
u8 255
i32 123
i64 9000
f32 1.5
f64 1.5
```

- Push exactly the named scalar type
- Can also use a constant of the same type

### `const`

In files:

```pdvm
const LIMIT 64
const i32 WIDTH 800
const f32 SPEED 2.5
const u8 ALPHA 255
```

In the console, `const` is also available interactively.

Rules:

- `const NAME VALUE` defaults to `i64`
- `const TYPE NAME VALUE` creates a typed constant
- Constants are looked up by exact name

## Numeric Behavior

Mixed arithmetic widens values with these rules:

0. If either operand is a float, the result is floating-point
0. Between floats, the higher float precision wins
0. Between integers, the higher integer capacity wins

Examples:

- `u8 + i32 -> i32`
- `i32 + i64 -> i64`
- `f32 + f64 -> f64`
- `i32 + f64 -> f64`

Division rules:

- If either operand is a float, `div` uses floating-point division
- Otherwise `div` uses integer division

Comparison and logical commands push `i64` boolean results:

- false: `0`
- true: `1`

`convert` uses C-style numeric casts.

## Structs

Define a struct:

```pdvm
struct Color u8 u8 u8 u8
struct Vector2 f32 f32
struct Rectangle f32 f32 f32 f32
```

Supported field types:

- `u8`
- `i32`
- `i64`
- `f32`
- `f64`
- `ptr`
- `structs`

Field layout rules:

- Each field is aligned to its natural alignment
- Struct alignment is the maximum field alignment
- Final struct size is padded up to struct alignment

### `pack`

```pdvm
u8 23
u8 56
u8 125
u8 255
pack Color
```

- Consumes the top `N` values where `N` is the field count
- Field `0` is the oldest of those consumed values
- Requires exact type matches
- Pushes one struct value

### `get`

```pdvm
get Color 0
```

- Pops a struct value
- Extracts the zero-based field
- Pushes that field as a normal typed value

### `set`

```pdvm
... struct-value new-field-value
set Color 2
```

- Pops the new field value
- Pops the struct value
- Updates the zero-based field
- Pushes the updated struct back

`get` and `set` require an exact struct name match.

## Memory

`pdvm` memory is addressed with non-negative integer addresses. Each address stores one full typed value, not just raw bytes.

Commands:

- `read`
- `write`

Usage:

```pdvm
push 123
push 0
write

push 0
read
print
```

Rules:

- Address values must be integer-like and non-negative
- Memory auto-expands as needed
- Reading a struct returns a cloned struct value
- Writing a struct stores a cloned struct value
- Pointer values are copied opaquely

## Stack and I/O Commands

### Program control

- `halt`
  - Stop execution

### Stack manipulation

- `pop` and `drop`
  - Pop and discard top value
- `dup`
  - Duplicate top value
- `dup2`
  - Duplicate top two values
- `swap`
  - Swap top two values
- `over`
  - Copy second value to top
- `rot`
  - Rotate top three values
- `nip`
  - Remove the second value from the top
- `tuck`
  - Duplicate top and tuck it under the next value
- `depth`
  - Push current stack depth as `i64`
- `clear`
  - Clear the stack
- `dump`
  - Debug-print the whole stack without consuming it
- `nl`
  - Print a newline to stdout

### Input

- `input`
  - Reads one numeric line from stdin
  - Pushes `i64` or `f64` depending on the input
- `inputs`
  - Reads one line from stdin
  - Pushes a string

### Output

- `print`
  - Print top scalar value without popping
- `emit`
  - Print top scalar value and pop it
- `printc`
  - Print top numeric value as a character without popping
- `emitc`
  - Print top numeric value as a character and pop it
- `prints`
  - Print a string without consuming its bytes
  - Pops only the length value
- `emits`
  - Print a string and consume it fully
- `error`
  - Print a string to stderr and consume it fully

Notes:

- `print` and `emit` do not support `struct` or `ptr`
- `dump` is the debug command for structs and pointers
- In the console, `print`, `printc`, `prints`, `dump`, `emit`, `emitc`, `emits`, and `error` add a trailing newline after the command completes

## Arithmetic, Comparison, and Logic

Arithmetic:

- `add`
- `sub`
- `mul`
- `div`
- `mod`
- `exp`
- `neg`

Comparison:

- `eq`
- `neq`
- `lt`
- `gt`
- `lte`
- `gte`

Logic:

- `and`
- `or`
- `not`

Notes:

- These commands operate on numeric scalar values only
- Results of comparisons and logic are `i64` booleans
- `exp` is implemented as repeated multiplication; it does not implement special handling for negative or fractional exponents

## `convert`

```pdvm
push 23
convert u8
```

- Pops the top numeric scalar
- Converts it to one of:
  - `u8`
  - `i32`
  - `i64`
  - `f32`
  - `f64`
- Pushes the converted value

Use this when a struct field or `dlcall` argument requires an exact type.

## Control Flow

Program-level control flow commands:

- `label NAME`
- `jmp NAME`
- `jz NAME`
- `jnz NAME`
- `jneg NAME`
- `jpos NAME`
- `jlez NAME`
- `jgez NAME`
- `je NAME`
- `jne NAME`
- `jl NAME`
- `jg NAME`
- `jle NAME`
- `jge NAME`
- `call NAME`
- `ret`

### Label rules

- Labels are scanned before execution starts
- `_main` is required
- `jmp`, `call`, and the jump commands target labels by exact name

### Stack behavior of jumps

- `jz`, `jnz`, `jneg`, `jpos`, `jlez`, `jgez` pop one numeric value
- `je`, `jne`, `jl`, `jg`, `jle`, `jge` pop two numeric values and compare them
- `call` pushes a return address on the internal call stack
- `ret` returns to the saved address

## Dynamic Libraries and FFI

### `dlopen`

```pdvm
dlopen raylib ./raylib.dll
```

- Loads a dynamic library and stores it under an alias

### `dlsym`

```pdvm
dlsym raylib.InitWindow raylib InitWindow
```

- Resolves a native symbol from a loaded library
- Stores it under a VM alias

### `dlclose`

```pdvm
dlclose raylib
```

- Unloads a library alias
- Removes symbols owned by that library

### `dlcall`

```pdvm
dlcall raylib.InitWindow v i32 i32 str
```

Syntax:

```text
dlcall <symbol-alias> <return-type> [arg-type ...]
```

Supported type tokens:

- Return:
  - `v`
  - `u8`
  - `i32`
  - `i64`
  - `f32`
  - `f64`
  - `ptr`
  - `str`
  - any defined struct name
- Arguments:
  - `u8`
  - `i32`
  - `i64`
  - `f32`
  - `f64`
  - `ptr`
  - `str`
  - any defined struct name

Argument rules:

- Arguments are consumed bottom-to-top, like `pack`
- Scalar arguments must match exactly
- Use `convert` when needed
- Struct arguments are passed by value using the struct layout already defined in `pdvm`
- `ptr` passes the raw `void *`

Return rules:

- `v` pushes nothing
- Scalar returns push the matching scalar type
- `ptr` pushes an opaque pointer value
- `str` copies a returned C string into string format
- Struct returns push a struct value

Current limits:

- Default platform C ABI only
- No variadic calls
- No user-facing pointer literal syntax yet

## Console Notes

Running `pdvm` without a file starts the console.

Console-only convenience:

- `const` works interactively
- `struct` works interactively
- library loading commands work interactively too

The file preprocessor and labels do not run in the console, so these are file-only features:

- `include`
- `@define`
- `@undef`
- `@if`
- `@elif`
- `@else`
- `@endif`
- `label`
- `call`
- `ret`
- `jmp`
- `jz`
- `jnz`
- `jneg`
- `jpos`
- `jlez`
- `jgez`
- `je`
- `jne`
- `jl`
- `jg`
- `jle`
- `jge`

## Examples

- [examples/hello_world.pdvm](../examples/hello_world.pdvm)
  - Minimal string output
- [examples/hello_world_box.pdvm](../examples/hello_world_box.pdvm)
  - Labels, calls, constants, and formatted output
- [examples/factorial.pdvm](../examples/factorial.pdvm)
  - Looping and arithmetic
- [examples/fibbonacci.pdvm](../examples/fibbonacci.pdvm)
  - Iterative numeric state on the stack
- [examples/2counter.pdvm](../examples/2counter.pdvm)
  - Memory-backed numeric algorithm
- [examples/multifile_test/main.pdvm](../examples/multifile_test/main.pdvm)
  - Multi-file source with `include`
- [examples/color_struct_packing.pdvm](../examples/color_struct_packing.pdvm)
  - Struct definition, `convert`, and `pack`
- [examples/brainfuck.pdvm](../examples/brainfuck.pdvm)
  - Larger interpreter-style program using memory, jumps, and string I/O
- [examples/raylib_test/raylib_test.pdvm](../examples/raylib_test/raylib_test.pdvm)
  - Cross-platform `dlcall` example using raylib

## Practical Notes

- `pushs` preserves trailing spaces in file scripts
- `prints` is non-destructive for the byte payload, but it does consume the top length value
- `emits` and `error` consume the full string payload
- `printc` and `emitc` convert numeric values to bytes which also work with unicode
- `dump` prints typed debug output such as:
  - `i64(10)`
  - `f64(2.5)`
  - `ptr(0x...)`
  - `Color{u8(23), u8(56), u8(125), u8(255)}`
