@page building_page Building the Library & Documentation

@section prerequisites prerequisites

@subsection prereq_all_platforms All Platforms:
- C++20 capable compiler (Clang, GCC, or MSVC)
- CMake >= 3.20

For Doxygen documentation:
- doxygen
- graphviz (for diagrams)

@subsection prereq_deb Linux (Debian/Ubuntu):
```zsh
sudo apt install build-essential cmake
```
For Doxygen documentation:
```zsh
sudo apt install doxygen graphviz
```
@subsection prereq_arch Linux (Arch):
```zsh
sudo pacman -S base-devel cmake
```
For Doxygen documentation:
```zsh
sudo pacman -S base-devel doxygen graphviz
```

@subsection prereq_windows Windows:
- Visual Studio 2019 or later with individual components:
   - MSBuild
   - Windows 11 SDK
   - MSBuild support for LLVM (clang-cl) toolset
   - C++ Clang compiler for Windows
   - MSVC v143 - VS 2022 C++ ARM build tools (Latest)
   - MSVC v143 - VS 2022 C++ ARM Spectre-mitigated libs (Latest)
   - MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools (Latest)
   - MSVC v143 - VS 2022 C++ ARM64/ARM64EC Spectre-mitigated libs (Latest)
   - MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
   - MSVC v143 - VS 2022 C++ x64/x86 Spectre-mitigated libs (Latest)
   - C++ Cmake tools for Windows
   - C++ Cmake tools for Linux
   (These are just the components i have installed, not all might be required and others might work)
- Cmake (can be installed manually or using visual studio)
- Ninja
```PowerShell
winget install Ninja-build.Ninja
```
- For Dxygen documentation:
   - Doxygen
   - graphviz
```PowerShell
winget install doxygen
winget install graphviz
```
- **Don't forget to add the graphvize binaries to PATH!** (usually located at `C:/Program Files/Graphviz/bin`)


@section add_to_project Adding statusbarlog to your project
The recommended approach to use this library is to include it in your project through a git submodule. However, directely using source files or precompiled libraries and header files from the [github releases page](https://github.com/lwidm/statusbarlog/releases) is also an option.

See [cmake submodule page](@ref cmake_module_page) for detailed instructions on how to inlcude statusbarlog in your project.

@section build_linux Building on Linux/macOS

```sh
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . -j$(nproc) --config Release
```

@section build_windows Building on Windows

@subsection windows_method1 Method 1: Using Visual Studio Developer Command Prompt
open "Developer Command Promt for VS 2022" or similar
```cmd
mkdir build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --config Release --parallel
```
@subsection windows_method2 Method 2: Using Ninja (Recommended)
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --parallel
```
@subsection windows_method3 Method 3: Using Visual Studio IDE
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Visual Studio 17 2022" -A x64
```
Then open the generated `.sln` in Visual Studio

@section cmake_options Important CMake Options

| Option | Type | Default | Values | Description |
|--------|------|---------|--------|-------------|
| `CMAKE_BUILD_TYPE` | STRING | `Release` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` | Standard CMake build type |
| `STATUSBARLOG_INSTALL` | BOOL | `OFF` | `ON`, `OFF` | Generate installation targets |
| `STATUSBARLOG_BUILD_TESTS` | BOOL | `OFF` | `ON`, `OFF` | Build test suite (uses GoogleTest) |
| `STATUSBARLOG_BUILD_TEST_MAIN` | BOOL | `OFF` | `ON`, `OFF` | Build test main executable |
| `STATUSBARLOG_LOG_LEVEL` | STRING | `kLogLevelDbg` | See below | Compile-time log level threshold |
| `STATUSBARLOG_SOURCE_MARKER` | STRING | `statusbarlog` | Any directory name | Directory name at which `__FILE__` is trimmed in log origins (e.g. `.../statusbarlog/src/foo.cc` → `src/foo.cc`) |
| `STATUSBARLOG_NO_AUTO_FLUSH` | BOOL | `OFF` | `ON`, `OFF` | Disable automatic flushing after each log message and statusbar update (faster, but output may be delayed) |

**`STATUSBARLOG_LOG_LEVEL` values** (from most to least restrictive):

| Value | Description |
|-------|-------------|
| `kLogLevelOff` | No logging at all |
| `kLogLevelErr` | Only errors |
| `kLogLevelWrn` | Errors and warnings |
| `kLogLevelInf` | Errors, warnings, and informational messages |
| `kLogLevelDbg` (default) | All messages including debug |

Messages with a level strictly above the configured threshold are discarded at compile time.

Example usage (release):
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
```

Example usage (development):
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTATUSBARLOG_LOG_LEVEL=kLogLevelDbg -DSTATUSBARLOG_BUILD_TESTS=On -DSTATUSBARLOG_BUILD_TEST_MAIN=On
```

Or when consuming via `add_subdirectory()`:
```cmake
set(STATUSBARLOG_LOG_LEVEL kLogLevelWrn CACHE STRING "statusbarlog default")
add_subdirectory(path/to/statusbarlog)
```

@section performance_notes_windows Performance Notes for Windows
- Use Ninja generator for fastest build times (actually ..., almost certainly irrelevant)
- MSVC compiler with */O2* and *LTO* provides best runtime performance (actually ..., almost certainly irrelevant)
- Consider *Profile-Guided Optimization (PGO)* for maximum performance in release builds (actually ..., almost certainly irrelevant)

@section generating_docs Generating Documentation

1. Ensure Doxygen and Graphviz are installed  
2. Adjust the Doxyfile to point to this mainpage:  
   ```
   INPUT = src include docs
   FILE_PATTERNS = *.h *.hpp *.cc *.cpp *.md *.h.in
   MAINPAGE = docs/mainpage.md
   EXCLUDE_PATTERNS = README.md
   EXTENSION_MAPPING = in=C++
   ```
   @note `EXTENSION_MAPPING = in=C++` is required. The public header is the
   template `include/statusbarlog/statusbarlog.h.in`, and Doxygen chooses its
   parser from the *last* file extension (`.in`), which it does not recognise as
   C++. Without this mapping the file is read but not parsed as C++, so
   `StatusbarHandle`, the `LogLevel` enum and the inline `Log` helpers are never
   documented and every `@ref` to them silently renders as plain text.
3. Run:
   ```sh
   doxygen Doxyfile
   ```

HTML output is usually in `docs/html`.
