// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2025 Lukas Widmer
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

// -- statusbarlog/test/src/statusbarlog_main.cc

// clang-format off

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "statusbarlog/sink.h"
#include "statusbarlog/statusbarlog.h"

// clang-format on

void PrintWithCleanup(const statusbar_log::sink::SinkHandle& sink_handle) {
  std::cout << "Start to be kept <- " << std::flush;
  ;
  statusbar_log::SaveCursorPosition(sink_handle);
  std::cout << "Temporary message that might be long" << std::flush;
  ;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  statusbar_log::RestoreCursorPosition(sink_handle);
  statusbar_log::ClearToEndOfLine(sink_handle);
  std::cout << "Clean message" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int body_statusbar(const statusbar_log::sink::SinkHandle& sink_handle,
                   statusbar_log::StatusbarHandle& h) {
  int err = statusbar_log::kStatusbarLogSuccess;
  const int total_steps1 = 15;
  const int total_steps2 = 100;

  for (std::size_t i = 0; i <= total_steps1; ++i) {
    double percent = static_cast<double>(i) / total_steps1 * 100.0;
    statusbar_log::UpdateStatusbar(h, 0, percent);
    if (i % 10 == 0 && i != 0) {
      LogInf(sink_handle, "10 Ticks reached");
    }
    if (i % 3 == 0 && i != 0) {
      // statusbar_log::UpdateStatusbar(h, 0, 100.1);
      // statusbar_log::UpdateStatusbar(h, 0, -0.1);
    }

    // Simulate work:
    for (std::size_t j = 0; j <= total_steps2; ++j) {
      double percent = static_cast<double>(j) / total_steps2 * 100.0;
      statusbar_log::UpdateStatusbar(h, 1, percent);
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
  }

  return err;
}

int main_body(const statusbar_log::sink::SinkHandle& sink_handle) {
  LogInf(sink_handle, "Starting test...");
  LogInf(sink_handle, "Starting test...");
  LogInf(sink_handle, "Starting test...");
  LogInf(sink_handle, "Starting test...");
  // statusbar_log::LogInf("ExremelyLongFilenameWhichShouldBeTruncated", "Lorem
  // ipsum dolor sit amet consectetur adipiscing elit. Quisque faucibus ex
  // sapien vitae pellentesque sem placerat. In id cursus mi pretium tellus duis
  // convallis. Tempus leo eu aenean sed diam urna tempor. Pulvinar vivamus
  // fringilla lacus nec metus bibendum egestas. Iaculis massa nisl malesuada
  // lacinia integer nunc posuere. Ut hendrerit semper vel class aptent taciti
  // sociosqu. Ad litora torquent per conubia nostra inceptos himenaeos.");

  int err = statusbar_log::kStatusbarLogSuccess;
  statusbar_log::StatusbarHandle h;

  std::cout << "\n\n";
  err = statusbar_log::CreateStatusbarHandle(
      h, sink_handle, {2, 1},                                  // <-- Postions
      {20, 10},                                                // <-- Bar widths
      {"first:  ", "second: "},                                // <-- prefixes
      {" -- 15 total steps", "           -- 100 total steps"}  // <-- postfixes
  );
  if (err != statusbar_log::kStatusbarLogSuccess) {
    LogErr(sink_handle, "Failed to create statusbar. Errorcode %d", err);
    return err;
  }

  err = body_statusbar(sink_handle, h);

  err = statusbar_log::DestroyStatusbarHandle(h, false);
  if (err != statusbar_log::kStatusbarLogSuccess) {
    LogErr(sink_handle, "Failed to destroy statusbar. Errorcode %d", err);
    return err;
  }

  return err;
}

int main() {
  std::cout << "\n ========== Starting main test program ==========\n"
            << std::endl;
  statusbar_log::sink::SinkHandle sink_handle;
  statusbar_log::sink::CreateSinkStdout(sink_handle);
  PrintWithCleanup(sink_handle);

  int err = main_body(sink_handle);

  std::cout.flush();
  std::fflush(stdout);
  statusbar_log::sink::DestroySinkHandle(sink_handle);

  statusbar_log::sink::SinkHandle sink_file_handle;
  statusbar_log::sink::CreateSinkFile(
      sink_file_handle,
      "/home/lukas/Documents/statusbarlog/build/main_test.txt");

  err = main_body(sink_file_handle);

  std::cout.flush();
  std::fflush(stdout);
  statusbar_log::sink::DestroySinkHandle(sink_file_handle);
  std::cout << "\n ========== Done with main test program ==========\n"
            << std::endl;
  return err;
}
