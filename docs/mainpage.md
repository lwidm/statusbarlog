@mainpage Overview

@section overview Overview

**StatusbarLog** is a C++ utility for simultaneous logging and multiple stacked statusbar displays in terminal applications.

Features:
- Multiple stacked statusbars with configurable text, sizes, and positions
- Logging with severity levels: `ERROR`, `WARN`, `INFO`, `DEBUG`
- Spinner animation for "busy" statusbars
- Cursor manipulation so log messages and statusbars do not overwrite each other
- Cross-platform design goals

@tableofcontents

@section guide Documentation pages

- @subpage usage_example_page : a runnable example and a step-by-step walkthrough.
- @subpage layout_model_page : how positions, groups and stacking actually work (read this to understand what `positions` mean).
- @subpage building_page : prerequisites, building the library, CMake options, and generating these docs.
- @subpage contributing_page : compilation database, code style and formatting guidelines.
