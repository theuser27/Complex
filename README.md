# Complex

Source for the spectral effect plugin I'm working on, directly inspired by DtBlkFx  

## Compiling

TL;DR If you just want to build the plugin as an end user just type the following in a console: 

Windows: `build.bat [vst3/clap] release`

macOS/Linux: `build.sh [vst3/clap] release [gcc/clang/zig]`

---

The full arguments available in order are `[vst3/clap/standalone/data] [debug/release]` and on Unix-likes `[gcc/clang/zig]`. By default this will build both vst3 and clap targets in debug mode. On Windows the scripts will use MSVC and on Unix-likes it will use whatever is bound to `$(CC)/$(CXX)` or whatever is provided on the CLI.

Needs a compiler with C++20 and C99 support.