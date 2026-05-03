# Complex

Source for the spectral effect plugin I'm working on, directly inspired by DtBlkFx  

## Compiling

TL;DR If you just want to build the plugin as an end user run `build.bat`/`build.sh` on Windows/Unix-likes respectively. This will build both VST3 and CLAP plugins at `build/(vst/clap)/release`

---

The full arguments available in order are `[vst3/clap/standalone/data] [debug/release]` and on Unix-likes `[gcc/clang/zig]`. Without any arguments this will build both vst3 and clap targets in release mode, otherwise `vst3` and `debug` are default. On Windows the scripts will use MSVC and on Unix-likes it will use whatever is bound to `$(CC)/$(CXX)` or whatever compiler is provided as an argument.

Needs a compiler with C++20 and C99 support.
