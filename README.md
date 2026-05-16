# kit

A single-file toolkit library for C.

## Overview

`kit.h` provides a collection of utilities commonly needed in C projects:

- **Logging** — configurable log levels and output sinks
- **String utilities** — lightweight non-owning string slices with common operations
- **Dynamic arrays** — macro-based generic vectors
- **Temporary memory allocator** — arena-style scratch memory with save/rewind
- **Process execution** — run commands, capture output, and manage child processes
- **Build utilities** — auto-rebuild and self-restart for development workflows

## Quick Start

### Using the Library

`kit.h` is a header-only library. Include it normally for declarations, then define `KIT_IMPLEMENTATION` in **exactly one** source file to include the implementations:

```c
#define KIT_IMPLEMENTATION
#include "kit.h"
```

### Running Tests

```bash
$ cc -o kit_test kit_test.c
$ ./kit_test
```

### Building a Shared Library

```bash
$ cc -x c -DKIT_IMPLEMENTATION -shared -fPIC -o libkit.so kit.h
```

### Auto-Rebuild Example

The `kit_auto_rebuild()` function enables a self-recompiling development workflow. At the start of `main()`, call it to automatically recompile and restart when the source changes:

```c
#define KIT_IMPLEMENTATION
#include "kit.h"

int main(int argc, char **argv) {
    if (!kit_auto_rebuild(argc, argv, __FILE__, "cc -o %s %s")) {
        return 1;
    }
    // Your program logic here...
    return 0;
}
```

Then simply run the binary and edit the source — it will rebuild and restart automatically.

## API Reference

### Logging

A simple logging system with configurable verbosity and output destinations.

#### Log Levels

| Level           | Description                    |
| --------------- | ------------------------------ |
| `KIT_LOG_DEBUG` | Verbose debugging information  |
| `KIT_LOG_INFO`  | General informational messages |
| `KIT_LOG_WARN`  | Warning messages               |
| `KIT_LOG_ERROR` | Error messages                 |
| `KIT_LOG_NONE`  | Suppress all log output        |

#### Log Sinks

| Sink           | Description                         |
| -------------- | ----------------------------------- |
| KIT_LOG_STDERR | Write to stderr (default)           |
| KIT_LOG_FILE   | Append to a file (with timestamps)  |
| KIT_LOG_CUSTOM | Forward to a user-provided callback |

#### Usage

```c
// Set minimum log level
kit_log_set_level(KIT_LOG_DEBUG);

// Log to a file
kit_log_set_sink(KIT_LOG_FILE, "app.log");

// Log to a custom handler
void my_handler(void *ctx, Kit_Log_Level level, const char *msg) {
    // Custom logging logic
}
kit_log_set_custom(my_handler, NULL);
```

Log messages are emitted via the internal `kit__log()` function. To add logging to your own code, you can use the same pattern:

```c
kit__log(KIT_LOG_INFO, "value is %d", 42);
```

> [!NOTE]
> When logging to a file, timestamps are automatically prepended.
> When logging to stderr, only the level tag is shown.

### String Utilities

`Kit_Str` is a non-owning string slice (pointer + length). It does **not** allocate or free memory — it references existing buffers.

#### Creating Kit_Str Values

```c
// From a string literal (length computed at compile time)
Kit_Str hello = KIT_STR("hello");

// From a null-terminated C-string
Kit_Str world = kit_str_from("world");

// From a buffer and explicit length
Kit_Str buf = kit_str_buf(my_buffer, my_length);
```

#### Printing Kit_Str

Use the format and argument macros with `printf`-style functions:

```c
printf("str: "KIT_STR_FMT"\n", KIT_STR_ARG(my_str));
// Expands to: printf("str: %.*s\n", (int)my_str.len, my_str.data);
```

#### Basic Operations

```c
kit_str_empty(str);                      // true if length == 0
kit_str_eq(a, b);                        // true if lengths and contents match
kit_str_eq_cstr(a, "hello");             // compare with a C-string
kit_str_starts_with(str, prefix);        // prefix check
kit_str_ends_with(str, suffix);          // suffix check
kit_str_find(str, 'a');                  // index of first 'a' (or str.len)
kit_str_rfind(str, 'b');                 // index of last 'b' (or str.len)
```

#### Slicing and Trimming

```c
Kit_Str sub = kit_str_slice(str, 2, 5);         // characters [2, 5]
Kit_Str left = kit_str_trim_left(str);          // trim leading whitespace
Kit_Str right = kit_str_trim_right(str);        // trim trailing whitespace
Kit_Str both = kit_str_trim(str);               // trim both ends
```

#### Splitting

`kit_str_split()` iterates over tokens separated by a delimiter. It modifies the input `Kit_Str` in place by advancing past each token:

```c
Kit_Str remaining = KIT_STR("foo,bar,baz");
Kit_Str token;
while (kit_str_split(&remaining, ',', &token)) {
    printf("token: " KIT_STR_FMT "\n", KIT_STR_ARG(token));
}
// Output:
// token: foo
// token: bar
// token: baz
```

### Dynamic Arrays (Vectors)

Macro-based generic dynamic arrays that work with any type.

#### Declaring and Initializing

```c
Kit_Arr(int) numbers = {0};        // zero-initialized (empty)
Kit_Arr(char*) names = {0};
```

#### Operations

```c
// Ensure capacity for at least 20 elements
kit_arr_reserve(&numbers, 20);

// Append elements
kit_arr_push(&numbers, 42);
kit_arr_push(&names, "hello");

// Access by index (bounds-checked; aborts on out-of-bounds)
int val = kit_arr_get(&numbers, 2);

// Pop last element (aborts if empty)
int last = kit_arr_pop(&numbers);

// Safe access (returns false if out-of-bounds or empty)
bool result;
if (kit_arr_try_get(&numbers, 2, &result)) {
    // Use result
}

// Free all memory
kit_arr_free(&numbers);
```

> [!NOTE]
> `Kit_Arr(T)` expands to a struct with `data`, `len`, and `cap` fields. You can inspect `len` and `cap` directly.

### Temporary Memory Allocator

An arena-style allocator for short-lived memory. All allocations are freed at once when `kit_temp_reset()` is called (also called automatically at program exit).

#### Basic Allocation

```c
void *buf = kit_temp_alloc(1024);                        // raw allocation
char *s1  = kit_temp_strdup("hello");                    // duplicate a C-string
char *s2  = kit_temp_strndup("hello", 3);                // duplicate first 3 chars ("hel")
char *s3  = kit_temp_sprintf("%s %d", "val", 42);        // formatted string
```

#### Save and Rewind

Use checkpoints to free only recent allocations without resetting everything:

```c
size_t checkpoint = kit_temp_save();

// ... allocate temporary data ...

kit_temp_rewind(checkpoint);  // frees everything allocated after the checkpoint
```

#### Full Reset

```c
kit_temp_reset();  // frees all temporary memory
```

> [!NOTE]
> The temporary allocator uses a linked list of blocks (default 4 KB each). Large allocations get their own block.

### Process Execution

Run external commands and capture their output.

#### Result Type

```c
typedef enum {
    KIT_OK = 0,           // success
    KIT_ERR_SPAWN,        // failed to start the process
    KIT_ERR_WAIT,         // failed to wait for the process
    KIT_ERR_SIGNAL,       // child killed by a signal
    KIT_ERR_ARGS,         // invalid arguments
    KIT_ERR_BUF,          // output buffer too small (truncated)
} Kit_Run_Status;

typedef struct {
    Kit_Run_Status status;
    int exit_code;        // child's exit code (if available)
    int signal_no;        // signal number (if killed by signal)
} Kit_Run_Result;
```

#### Running Commands

```c
// Run a shell command
Kit_Run_Result r = kit_run("ls -l");
if (r.status != KIT_OK) {
    fprintf(stderr, "Error: %s\n", kit_strerror(r.status));
}

// Run with an argument array (no shell involved)
const char *args[] = {"grep", "pattern", "file.txt", NULL};
r = kit_run_argv(args);

// Capture stdout into a buffer
char buf[1024];
r = kit_run_capture("echo hello", buf, sizeof(buf));
if (r.status == KIT_OK) {
    printf("Output: %s\n", buf);
}
```

#### Error Messages

```c
const char *msg = kit_strerror(r.status);
// Returns: "success", "failed to spawn child process", etc.
```

> [!NOTE]
> `kit_run()` uses `system()` (shell-based), while `kit_run_argv()` uses `fork()`/`execvp()` directly (no shell interpretation).

### Build Utilities

Utilities for implementing self-recompiling programs during development.

#### Functions

```c
// Check if source is newer than binary
bool needs;
if (kit_needs_rebuild("main.c", "main", &needs) && needs) {
    // Recompile...
}

// Recompile using a printf-style template
// The template receives (binary, source) in that order.
kit_rebuild("main.c", "main", "cc %s -o %s");

// Check, rebuild, and restart — typically called at the start of main()
kit_auto_rebuild(argc, argv, "main.c", "cc -o %s %s");
```

#### How `kit_auto_rebuild` Works

1. Compares the modification time of the source file and the running binary.
2. If the source is newer, compiles it using the provided template.
3. If compilation succeeds, replaces the current process with the new binary via execv().
4. If no rebuild is needed, returns true and the program continues normally.

> [!NOTE]
> The `cc_template` format string takes `(binary, source)` as its two arguments. For example: "cc -o %s %s" expands to "cc -o main main.c"

## Inspirations

The design of this project was heavily inspired by:

- [Sean Barret's stb](https://github.com/nothings/stb) - Single-file public domain C/C++ libraries
- [Tsoding's nob.h](https://github.com/tsoding/nob.h) - Header only library for writing build recipes in C

## License

This library is released under the [MIT License](https://opensource.org/license/MIT).
