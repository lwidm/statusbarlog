@page usage_example_page Usage Example

Example code snippet (from `docs/example/main.cc`):

@include docs/example/main.cc

@section brief Brief explanation

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

3. **Now a simple log message can be done like:**
   ```cpp
   LogDbg(sink_handle, "Funny debug message");
   LogInf(sink_handle, "Starting test...");
   LogWrn(sink_handle, "Couldn't obtain viscosity. Using 1.6e-5 m^2/s");
   LogErr(sink_handle, "Failed to compute rhs");
   ```
   The `Log*` macros stamp the current `__FILE__` and `__LINE__` into each
   message automatically, so no per-file filename constant is needed. They are
   macros, so call them unqualified (not `statusbar_log::LogInf`).

4. **Create a stacked statusbar** (here: two bars in one group, on top of each other)
   ```cpp
   statusbar_log::StatusbarHandle handle;
   int err_code = statusbar_log::CreateStatusbarHandle(
       handle, sink_handle,
       {2, 1},                    // relative positions in this group (2 = upper)
       {20, 10},                  // bar widths
       {"first", "second"},       // prefixes
       {"20 long", "10 long"});   // postfixes
   ```
   The positions are ranks *within this group*, not absolute screen rows. See
   @ref layout_model_page for what that means and how multiple groups stack.

5. **Updating a statusbar**
   ```cpp
   statusbar_log::UpdateStatusbar(handle, 0, percent);  // top bar
   statusbar_log::UpdateStatusbar(handle, 1, percent);  // lower bar
   ```
   Note: For printing the statusbar the first time just use the percentage 0.

6. **Log while updating**
   ```cpp
   LogInf(sink_handle, "10 ticks reached");
   ```
   The log messages are printed above any active statusbars.

8. **Cleanup**
   ```cpp
   statusbar_log::DestroyStatusbarHandle(handle);
   statusbar_log::sink::DestroySinkHandle(sink_handle);
   ```
