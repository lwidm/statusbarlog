@mainpage Quickstart / Overview

@section overview Overview

**StatusbarLog** is a C++ utility for simultaneous logging and multiple stacked statusbar displays in terminal applications.

Features:
- Multiple stacked statusbars with configurable text, sizes, and positions
- Logging with severity levels: `ERROR`, `WARN`, `INFO`, `DEBUG`
- Spinner animation for "busy" statusbars
- Cursor manipulation so log messages and statusbars do not overwrite each other
- Cross-platform design goals

@tableofcontents

@section usage_example Usage Example

Example code snippet (from `docs/example/main.cc`):

@include docs/example/main.cc

@subsection brief Brief explanation

1. **Set compile-time log level (optional)**
   The log level is set by CMake when generating the header. This ensures only log messages with higher or equal log priority (ERROR, WARNING, INFO, DEBUG) to the log level will be printed.
   Override via:
   ```sh
   cmake -DSTATUSBARLOG_LOG_LEVEL=kLogLevelWrn ...
   ```
   (all options: `kLogLevelDbg`, `kLogLevelInf`, `kLogLevelWrn`, `kLogLevelErr`, `kLogLevelOff`)

2. **Create a sink** (stdout or file)
   ```cpp
   statusbar_log::sink::SinkHandle sink_handle;
   statusbar_log::sink::CreateSinkStdout(sink_handle);
   // Or for a file sink:
   // statusbar_log::sink::CreateSinkFile(sink_handle, "output.txt");
   ```

3. **Define filename for every cpp file in which you want to log**
   ```cpp
   const std::string kFilename = "StatusbarLog_main.cc";
   ```

4. **Now a simple log message can be done like:**
   ```cpp
   statusbar_log::LogDbg(kFilename, sink_handle, "Funny debug message");
   statusbar_log::LogInf(kFilename, sink_handle, "Starting test...");
   statusbar_log::LogWrn(kFilename, sink_handle, "Couldn't obtain viscosity. Using 1.6e-5 m^2/s");
   statusbar_log::LogErr(kFilename, sink_handle, "Failed to compute rhs");
   ```

5. **Create a stacked statusbar** (here: two statusbars on top of each other)
   ```cpp
   statusbar_log::StatusbarHandle handle;
   int err_code = statusbar_log::CreateStatusbarHandle(
       handle, sink_handle,
       {2, 1},                    // positions
       {20, 10},                  // bar widths
       {"first", "second"},       // prefixes
       {"20 long", "10 long"});   // postfixes
   ```

6. **Updating a statusbar**
   ```cpp
   statusbar_log::UpdateStatusbar(handle, 0, percent);  // top bar
   statusbar_log::UpdateStatusbar(handle, 1, percent);  // lower bar
   ```
   Note: For printing the statusbar the first time just use the percentage 0.

7. **Log while updating**
   ```cpp
   statusbar_log::LogInf(kFilename, sink_handle, "10 ticks reached");
   ```
   The log messages are printed above any active statusbars.

8. **Cleanup**
   ```cpp
   statusbar_log::DestroyStatusbarHandle(handle);
   statusbar_log::sink::DestroySinkHandle(sink_handle);
   ```

@section building Building

@subsection prerequisites prerequisites

@subsubsection prereq_all_platforms All Platforms:
- C++20 capable compiler (Clang, GCC, or MSVC)
- CMake >= 3.20

For Doxygen documentation:
- doxygen
- graphviz (for diagrams)

@subsubsection prereq_deb Linux (Debian/Ubuntu):
```zsh
sudo apt install build-essential cmake
```
For Doxygen documentation:
```zsh
sudo apt install doxygen graphviz
```
@subsubsection prereq_arch Linux (Arch):
```zsh
sudo pacman -S base-devel cmake
```
For Doxygen documentation:
```zsh
sudo pacman -S base-devel doxygen graphviz
```

@subsubsection prereq_windows Windows:
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


@subsection add_to_project Adding statusbarlog to your project
The recommended approach to use this library is to include it in your project through a git submodule. However, directely using source files or precompiled libraries and header files from the [github releases page](https://github.com/lwidm/statusbarlog/releases) is also an option.

See [cmake submodule page](@ref cmake_module_page) for detailed instructions on how to inlcude statusbarlog in your project.

@subsection build_linux Building on Linux/macOS

```sh
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . -j$(nproc) --config Release
```

@subsection build_windows Building on Windows

@subsubsection windows_method1 Method 1: Using Visual Studio Developer Command Prompt
open "Developer Command Promt for VS 2022" or similar
```cmd
mkdir build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --config Release --parallel
```
@subsubsection windows_method2 Method 2: Using Ninja (Recommended)
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --parallel
```
@subsubsection windows_method3 Method 3: Using Visual Studio IDE
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Visual Studio 17 2022" -A x64
```
Then open the generated `.sln` in Visual Studio

@subsection cmake_options Important CMake Options

| Option | Type | Default | Values | Description |
|--------|------|---------|--------|-------------|
| `CMAKE_BUILD_TYPE` | STRING | `Release` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` | Standard CMake build type |
| `STATUSBARLOG_INSTALL` | BOOL | `OFF` | `ON`, `OFF` | Generate installation targets |
| `STATUSBARLOG_BUILD_TESTS` | BOOL | `OFF` | `ON`, `OFF` | Build test suite (uses GoogleTest) |
| `STATUSBARLOG_BUILD_TEST_MAIN` | BOOL | `OFF` | `ON`, `OFF` | Build test main executable |
| `STATUSBARLOG_LOG_LEVEL` | STRING | `kLogLevelDbg` | See below | Compile-time log level threshold |

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

@subsection performance_notes_windows Performance Notes for Windows
- Use Ninja generator for fastest build times (actually ..., almost certainly irrelevant)
- MSVC compiler with */O2* and *LTO* provides best runtime performance (actually ..., almost certainly irrelevant)
- Consider *Profile-Guided Optimization (PGO)* for maximum performance in release builds (actually ..., almost certainly irrelevant)

@section generating_docs Generating Documentation

1. Ensure Doxygen and Graphviz are installed  
2. Adjust the Doxyfile to point to this mainpage:  
   ```
   INPUT = src include docs
   FILE_PATTERNS = *.h *.hpp *.cc *.cpp *.md
   MAINPAGE = docs/mainpage.md
   EXCLUDE_PATTERNS = README.md
   ```
3. Run:
   ```sh
   doxygen Doxyfile
   ```

HTML output is usually in `docs/html`.

@section contributing_style Contributing & Style 

@subsection compilation_database Compilation database
I used the compilation database located at `compile_commands.json.in` together with the `clangd` lsp for development and this works very well. I recommend using a build system that supports generating this compilation database (like _make_ or _Ninja_). If a build system that supports it is used cmake will generate the compilation database for you and coppy it to the root directory. If one doesn't want to do this one can coppy the `compile_commands.json.windows` or `compile_commands.json.linux` to `compile_commands.json` and place it in the root directory. This should work just fine as long as no new files or defines are created.

@subsubsection style_guidelines Style & Formatting Guidelines
- Follow [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html) as strictly as possible 
- Add ApacheAdd Apache-2.0 License boilerpalte at the top of every source file (**replace year and owner**):
@code{cpp}
// SPDX-License-Identifier: Apache-2.0
// Copyright (c) [yyyy] [name of copyright owner]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// -- statusbarlog/rest/of/path.cc
@endcode
- Use `clang-format` with the provided root-level `.clang-format`. Ideally use
  an editor integration to format on save. For exceptions, wrap the unformatted
  region with:
   @code{cpp}
   // clang-format off
   ...
   // clang-format on
   @endcode
   Don't forget to re-enalbe!



