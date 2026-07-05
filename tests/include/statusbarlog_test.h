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

// -- statusbarlog/test/include/statusbarlog_test.h

#ifndef STATUSBARLOG_STATUSBARLOG_TEST_H_
#define STATUSBARLOG_STATUSBARLOG_TEST_H_

#include <fcntl.h>
#include <sys/stat.h>

#include <atomic>
#include <cstring>

#include "statusbarlog/statusbarlog.h"

namespace statusbar_log {
namespace test {

inline std::atomic<unsigned int> _is_capturing = 0;

constexpr std::string test_output_dir = "test_output";
constexpr bool kSeparateLogFiles = false;
constexpr std::string global_log_filename = "test_log.txt";
inline int _saved_stdout_fd = -1;
inline int _saved_pipe_read_fd = -1;

void SetupTestOutputDirectory();

std::string GenerateTestLogFilename(const std::string& test_suite,
                                    const std::string& test_name);

int RedirectCreateStatusbarHandle(
    statusbar_log::StatusbarHandle& statusbar_handle,
    const statusbar_log::sink::SinkHandle& sink_handle,
    const std::vector<unsigned int> _positions,
    const std::vector<unsigned int> _bar_sizes,
    const std::vector<std::string> _prefixes,
    const std::vector<std::string> _postfixes, const std::string& log_filename);

int RedirectDestroyStatusbarHandle(
    statusbar_log::StatusbarHandle& statusbar_handle,
    const std::string& log_filename);

int RedirectUpdateStatusbar(statusbar_log::StatusbarHandle& statusbar_handle,
                            const std::size_t idx, const double percent,
                            const std::string& log_filename);

int RedirectToStrUpdateStatusbar(
    std::string& capture_stdout,
    statusbar_log::StatusbarHandle& statusbar_handle, const std::size_t idx,
    const double percent);

int RedirectToStrLog(std::string& capture_stdout,
                     const statusbar_log::sink::SinkHandle& sink_handle,
                     LogLevel log_level, const std::string filename,
                     const char* fmt, ...);

std::string StripAnsiEscapeSequences(const std::string& s);

int CaptureStdoutToPipe();
int RestoreCaptureStdoutToStr(std::string& capture_stdout);

}  // namespace test
}  // namespace statusbar_log

#endif  // STATUSBARLOG_STATUSBARLOG_TEST_H_
