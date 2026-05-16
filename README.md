# kit

A toolkit library for C.

## Quick Start

Check the comments in [kit.h](kit.h) to see how to use this library.

TODO: write some examples, for now you can check [test_kit.c](test_kit.c).

### Run tests

```bash
$ cc -o kit_test kit_test.c
$ ./kit_test
```

### Compile ragebait

```bash
$ cc -x c -DKIT_IMPLEMENTATION -shared -fPIC -o libkit.so kit.h
$ rustc -L. -o ragebait ragebait.rs
$ LD_LIBRARY_PATH=. ./ragebait
```

You can then modify `ragebait.rs` and it will recompile itself!

## Inspirations

The idea if this project was heavily inspired by [Sean Barrett's stb](https://github.com/nothings/stb) and [Tsoding's nob.h](https://github.com/tsoding/nob.h) libraries.
