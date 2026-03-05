<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (c) 2025 Lukas Widmer -->

# StatusbarLog

StatusbarLog is a C++ utility for simultaneous logging and multiple stacked statusbar displays in terminal applications.

## Quick Links
- [Features](#features)
- [Documentation](#documentation)
- [Building](#building)
  - [Prerequisites](#prerequisites)
    - [All Platforms](#all-platforms)
    - [Linux (Debian/Ubuntu)](#linux-debianubuntu)
    - [Linux (Arch)](#linux-arch)
    - [Windows](#windows)
  - [Building on Linux/macOS](#building-on-linuxmacos)
  - [Adding statusbarlog to your project](#adding-statusbarlog-to-your-project)
  - [Building on Windows](#building-on-windows)
  - [Important CMake Options](#important-cmake-options)
  - [Performance Notes for Windows](#performance-notes-for-windows)
- [Contributing & Style](#contributing--style)
  - [Compilation database](#compilation-database)
  - [Style & Formatting Guidelines](#style--formatting-guidelines)
- [Using StatusbarLog as a CMake Module in Your Project](#using-statusbarlog-as-a-cmake-module-in-your-project)
- [License](#license)
- [TODO](#todo)

## Features

- Multiple stacked statusbars with configurable text, sizes, and positions
- Logging with severity levels: ERROR, WARN, INFO, DEBUG
- Spinner animation for "busy" statusbars
- Cursor manipulation so log messages and statusbars do not overwrite each other
- Cross-platform design goals

## Documentation

Full documentation is available via Doxygen. To generate documentation locally:
```zsh
doxygen Doxyfile
```
HTML output is usually in docs/html.

For online documentation hosted via GitHub Pages, see the project's [GitHub Pages site](https://lwidm.github.io/statusbarlog).


## Building

### prerequisites

#### All Platforms:
- C++20 capable compiler (Clang, GCC, or MSVC)
- CMake >= 3.20

For Doxygen documentation:
- doxygen
- graphviz (for diagrams)

#### Linux (Debian/Ubuntu):
```zsh
sudo apt install build-essential cmake
```
For Doxygen documentation:
```zsh
sudo apt install doxygen graphviz
```

#### Linux (Arch):
```zsh
sudo pacman -S base-devel cmake
```
For Doxygen documentation:
```zsh
sudo pacman -S base-devel doxygen graphviz
```

#### Windows:
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

### Building on Linux/macOS

```sh
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . -j$(nproc) --config Release
```

### Adding statusbarlog to your project
The recommended approach to use this library is to include it in your project through a git submodule. However, directely using source files or precompiled libraries and header files from the [github releases page](https://github.com/lwidm/statusbarlog/releases) is also an option.

See [cmake submodule page](@ref cmake_module_page) for detailed instructions on how to include statusbarlog in your project.

### Building on Windows

#### Method 1: Using Visual Studio Developer Command Prompt
open "Developer Command Promt for VS 2022" or similar
```cmd
mkdir build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --config Release --parallel
```

#### Method 2: Using Ninja (Recommended)
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelInf
cmake --build . --parallel
```
#### Method 3: Using Visual Studio IDE
```cmd
mkdir build
cd build
cmake -S .. -B . -G "Visual Studio 17 2022" -A x64
```
Then open the generated `.sln` in Visual Studio

### Important CMake Options

| Option | Type | Default | Description |
|--------|------|----------|--------------|
| CMAKE_BUILD_TYPE | STRING | Release | Standard CMake build type |
| STATUSBARLOG_INSTALL | BOOL | OFF | Generate installation targets |
| STATUSBARLOG_BUILD_TESTS | BOOL | OFF | Build test suite |
| STATUSBARLOG_BUILD_TEST_MAIN | BOOL | OFF | Build test main executable |
| STATUSBARLOG_LOG_LEVEL | STRING | kLogLevelDbg | Compile-time log level (kLogLevelOff, kLogLevelErr, kLogLevelWrn, kLogLevelInf, kLogLevelDbg) |

Example usage:
```zsh
cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DSTATUSBARLOG_LOG_LEVEL=kLogLevelDbg
```
Or when consuming via add_subdirectory():
```cmake
set(STATUSBARLOG_LOG_LEVEL kLogLevelWrn CACHE STRING "statusbarlog default")
add_subdirectory(path/to/statusbarlog)
```

### Performance Notes for Windows
- Use Ninja generator for fastest build times
- MSVC compiler with */O2* and *LTO* provides best runtime performance
- Consider *Profile-Guided Optimization (PGO)* for maximum performance in release builds

## Contributing & Style 
### Compilation database
I used the compilation database located at `compile_commands.json.in` together with the `clangd` lsp for development and this works very well. I recommend using a build system that supports generating this compilation database (like _make_ or _Ninja_). If a build system that supports it is used cmake will generate the compilation database for you and coppy it to the root directory. If one doesn't want to do this one can coppy the `compile_commands.json.windows` or `compile_commands.json.linux` to `compile_commands.json` and place it in the root directory. This should work just fine as long as no new files or defines are created.
### Style & Formatting Guidelines
- Follow [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html) as strictly as possible 
- Add ApacheAdd Apache-2.0 License boilerpalte at the top of every source file (**replace year and owner**):
```cpp
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
```
- Use `clang-format` with the provided root-level `.clang-format`. Ideally use
  an editor integration to format on save. For exceptions, wrap the unformatted
  region with:
   ```cpp
   // clang-format off
   ...
   // clang-format on
   ```
   Don't forget to re-enalbe!

## Using StatusbarLog as a CMake Module in Your Project

You can include **StatusbarLog** in your own C++ project in a few ways, I use it as a **git submodule** or by directly adding it to your project tree. Once included, you can consume it via CMake.

### 1. Include StatusbarLog source files:

#### a) As a git submodule (Recommended)
The recommended approach is to add statusbarlog as a git submodule:
```zsh
git submodule add git@github.com:lwidm/statusbarlog.git statusbarlog
git submodule update --init --recursive
```

#### b) Directly using release files (Not Recommeded)
You can find packaged release artifacts (archives and prebuilt libraries) on the GitHub Releases page for this project: [https://github.com/lwidm/statusbarlog/releases](https://github.com/lwidm/statusbarlog/releases)

Typical ways to consume release files:
- **Use prebuilt binaries / libraries manually**: Download the release assets (headers + static library .a or .lib, and any example binaries) and:
   1. Add the include/ folder from the release to your compiler include path.
   2. Link the provided static library into your project (e.g. add the .a/.lib to your linker inputs).
   3. Ensure the library's compile-time log level matches your needs. If not, build from source.

- **Extract the archive into your project or add as a submodule**: Extract the `statusbarlog-*.tar.gz` into a statusbarlog/ folder inside your repository and follow the rest of this guide.

### 2. Add it to your CMake project
in your root `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: set default StatusbarLog log level
set(STATUSBARLOG_LOG_LEVEL kLogLevelInf CACHE STRING "Default StatusbarLog log level")

# Include StatusbarLog
add_subdirectory(statusbarlog)

# Your executable
add_executable(${PROJECT_NAME} main.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE statusbarlog)
@endcode

@subsection cmake_module_build 3. Build your project:
@code{bash}
mkdir -p build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

@subsection cmake_module_use 4. Use StatusbarLog in your code:
```cpp
#include "statusbarlog/statusbarlog.h"

int main() {
    statusbarlog::LogInf("main.cc", "Starting application...");
}
```

### Notes
- This approach allows you to keep StatusbarLog version-controlled alongside your project via the submodule,
- Or to copy the source folder directly into your project.
- When cloning your project elsewhere, run git submodule update --init --recursive if you use the submodule method.

## License
This project is licensed under the Apache License, Version 2.0. See the top-level LICENSE file for details.

## TODO
- Let log messages and statusbars take up arbitrary streams
- Optionally don't force flushing after every status message or every statusbarupdate
  - make it togalable using cmake

## TODO unittest
- out of bounds `spin_idx`, log message length, status bar text, `statusbar_registry`
- Thread safety (test race conditions)
- Test unsanitised strings (in log and prefixes and postfixes)
- Test bounds of log message, filename, prefix and postfix length
- Test bounds on statusbar_registry and statusbar_free_handles

### TEST PRIORITY LIST

**MEDIUM PRIORITY - String & Utility Functions**
4. String Sanitization Tests 
```cpp
TEST(StringSanitization, SanitizeString_NoChangesNeeded)
TEST(StringSanitization, SanitizeString_ControlCharacters)
TEST(StringSanitization, SanitizeString_UnicodeReplacement)
TEST(StringSanitization, SanitizeStringWithNewline_PreservesNewlines)
TEST(StringSanitization, SanitizeString_TruncatesLongStrings)
TEST(StringSanitization, SanitizePrefixPostfixLengthLimits)
```

5. Terminal Utility Tests
```cpp
TEST(TerminalUtils, GetTerminalWidth_Success)
TEST(TerminalUtils, GetTerminalWidth_FallbackToDefault)
TEST(TerminalUtils, ClearCurrentLine_NoCrash)
TEST(TerminalUtils, CursorPosition_SaveRestore)
TEST(TerminalUtils, FlushOutput_Behavior)
```

**LOW PRIORITY - Complex Scenarios**
6. Logging Integration Tests
```cpp
TEST(Logging, LogWithActiveStatusbars)
TEST(Logging, LogDifferentLevels)
TEST(Logging, LogWithFormatting)
TEST(Logging, LogWithLongMessages_Truncation)
```

7. Concurrent Access Tests

```cpp
TEST(Concurrency, MultipleHandlesSimultaneousCreation)
TEST(Concurrency, UpdatesFromDifferentThreads_NoCrash)
TEST(Concurrency, LogWhileStatusbarActive)
```

8. Edge Case & Stress Tests
```cpp
TEST(EdgeCases, RapidSequentialUpdates)
TEST(EdgeCases, ManyStatusbarsSimultaneously)
TEST(EdgeCases, EmptyVectors_ErrorHandling)
TEST(EdgeCases, VeryLongPrefixPostfix_Truncation)
TEST(EdgeCases, BoundaryPercentages_0_and_100)
```

**SPECIFIC TESTABLE BEHAVIORS**
For _DrawStatusbarComponent:
```cpp
TEST(DrawStatusbar, FormatsCorrectly_0Percent)
TEST(DrawStatusbar, FormatsCorrectly_50Percent)
TEST(DrawStatusbar, FormatsCorrectly_100Percent)
TEST(DrawStatusbar, SpinnerCyclesCorrectly)
TEST(DrawStatusbar, BarWidthRespected)
TEST(DrawStatusbar, TerminalWidthTruncation)
TEST(DrawStatusbar, ErrorCodes_AllScenarios)
```

For _GetTerminalWidth:
```cpp
TEST(TerminalWidth, DefaultsTo80OnFailure)
TEST(TerminalWidth, PlatformSpecificBehavior)
```

For CreateStatusbarHandle:
```cpp
TEST(CreateHandle, ReusesFreeHandles)
TEST(CreateHandle, SanitizesInputStrings)
TEST(CreateHandle, InitializesAllComponentsToZero)
TEST(CreateHandle, SetsHandleValidAndID)
```

For UpdateStatusbar:
```cpp
TEST(UpdateStatusbar, SpinnerIndexIncrements)
TEST(UpdateStatusbar, ErrorReportedFlagSetsOnce)
TEST(UpdateStatusbar, NoErrorOnSubsequentUpdatesAfterError)
```
