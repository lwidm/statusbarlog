// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2025 Lukas Widmer

// -- statusbarlog/docs/example/main.cc

// clang-format off

#include <cstddef>
#include <iostream>
#include <string>
#include <thread>

#include "statusbarlog/sink.h"
#include "statusbarlog/statusbarlog.h"

// clang-format on

const std::string kFilename = "main.cc";

int main() {
  // 1. Create a sink (stdout or file)
  statusbar_log::sink::SinkHandle sink_handle;
  statusbar_log::sink::CreateSinkStdout(sink_handle);

  // 2. Log messages
  statusbar_log::LogInf(kFilename, sink_handle, "Starting example...");

  // 3. Create a stacked statusbar (two bars)
  std::cout << "\n\n";
  statusbar_log::StatusbarHandle h;
  int err = statusbar_log::CreateStatusbarHandle(
      h, sink_handle,
      {2, 1},                                               // <-- Positions
      {20, 10},                                              // <-- Bar widths
      {"first:  ", "second: "},                              // <-- Prefixes
      {" -- 15 total steps", "           -- 100 total steps"}  // <-- Postfixes
  );
  if (err != statusbar_log::kStatusbarLogSuccess) {
    statusbar_log::LogErr(kFilename, sink_handle,
                          "Failed to create statusbar. Errorcode %d", err);
    return err;
  }

  // 4. Update statusbars and log while they are active
  const int total_steps1 = 15;
  const int total_steps2 = 100;
  for (std::size_t i = 0; i <= total_steps1; ++i) {
    double percent = static_cast<double>(i) / total_steps1 * 100.0;
    statusbar_log::UpdateStatusbar(h, 0, percent);

    if (i % 10 == 0 && i != 0) {
      statusbar_log::LogInf(kFilename, sink_handle, "10 Ticks reached");
    }

    for (std::size_t j = 0; j <= total_steps2; ++j) {
      double percent = static_cast<double>(j) / total_steps2 * 100.0;
      statusbar_log::UpdateStatusbar(h, 1, percent);
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
  }

  // 5. Destroy the statusbar handle when done
  err = statusbar_log::DestroyStatusbarHandle(h);
  if (err != statusbar_log::kStatusbarLogSuccess) {
    statusbar_log::LogErr(kFilename, sink_handle,
                          "Failed to destroy statusbar. Errorcode %d", err);
    return err;
  }

  // 6. Destroy the sink handle
  statusbar_log::sink::DestroySinkHandle(sink_handle);
  return 0;
}
