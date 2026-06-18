@page contributing_page Contributing & Style

@section compilation_database Compilation database
I used the compilation database located at `compile_commands.json.in` together with the `clangd` lsp for development and this works very well. I recommend using a build system that supports generating this compilation database (like _make_ or _Ninja_). If a build system that supports it is used cmake will generate the compilation database for you and coppy it to the root directory. If one doesn't want to do this one can coppy the `compile_commands.json.windows` or `compile_commands.json.linux` to `compile_commands.json` and place it in the root directory. This should work just fine as long as no new files or defines are created.

@section style_guidelines Style & Formatting Guidelines
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
