@page cmake_module_page StatusbarLog as CMake (Sub-)Module

You can include **StatusbarLog** in your own C++ project in a few ways, I use it as a **git submodule** or by directly adding it to your project tree. Once included, you can consume it via CMake.

@section source_files 1. Include StatusbarLog source files:

@subsection git_submodule a) As a git submodule (Recommended)
The recommended approach is to add statusbarlog as a git submodule:
@code{bash}
git submodule add git@github.com:lwidm/statusbarlog.git statusbarlog
git submodule update --init --recursive
@endcode

@subsection release_files b) Directly using release files (Not Recommeded)
You can find packaged release artifacts (archives and prebuilt libraries) on the GitHub Releases page for this project: [https://github.com/lwidm/statusbarlog/releases](https://github.com/lwidm/statusbarlog/releases)

Typical ways to consume release files:
- **Use prebuilt binaries / libraries manually**: Download the release assets (headers + static library .a or .lib, and any example binaries) and:
   1. Add the include/ folder from the release to your compiler include path.
   2. Link the provided static library into your project (e.g. add the .a/.lib to your linker inputs).
   3. Ensure the library's compile-time log level matches your needs. If not, build from source.

- **Extract the archive into your project or add as a submodule**: Extract the `statusbarlog-*.tar.gz` into a statusbarlog/ folder inside your repository and follow the rest of this guide.

@section cmake_module_consume 2. Add it to your CMake project
in your root `CMakeLists.txt`:
@code{cmake}
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

@section cmake_module_build 3. Build your project:
@code{bash}
mkdir -p build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
@endcode

@section cmake_module_use 4. Use StatusbarLog in your code:
@code{cpp}
#include "statusbarlog/statusbarlog.h"

int main() {
    statusbar_log::sink::SinkHandle sink_handle;
    statusbar_log::sink::CreateSinkStdout(sink_handle);
    LogInf(sink_handle, "Starting application...");
}
@endcode

@section cmake_module_notes Notes
- This approach allows you to keep StatusbarLog version-controlled alongside your project via the submodule.
- When cloning your project elsewhere, run git submodule update --init --recursive if you use the submodule method.
