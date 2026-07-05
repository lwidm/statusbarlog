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

// -- statusbarlog/test/src/statusbarlog_test_helpers.h

// clang-format off


#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>

#include "statusbarlog/os_posix.h"
#include "statusbarlog/sink.h"
#include "statusbarlog/statusbarlog.h"
#include "statusbarlog_test.h"

// clang-format on

namespace statusbar_log {
namespace test {

void SetupTestOutputDirectory() {
  if (kSeparateLogFiles) {
    std::filesystem::remove_all(test_output_dir);
    if (!std::filesystem::create_directory(test_output_dir)) {
      std::cerr << "Failed to create test output directory: " << test_output_dir
                << std::endl;
    }
  }
  if (!std::filesystem::remove_all(global_log_filename)) {
    std::cerr << "Failed to create test log file: " << global_log_filename
              << std::endl;
  }
}

std::string GenerateTestLogFilename(const std::string& test_suite,
                                    const std::string& test_name) {
  if (kSeparateLogFiles) {
    std::string safe_suite = test_suite;
    std::string safe_name = test_name;
    std::replace(safe_suite.begin(), safe_suite.end(), '/', '_');
    std::replace(safe_name.begin(), safe_name.end(), '/', '_');
    std::replace(safe_suite.begin(), safe_suite.end(), '\\', '_');
    std::replace(safe_name.begin(), safe_name.end(), '\\', '_');
    return test_output_dir + "/" + safe_suite + "_" + safe_name + ".log";
  }
  std::string safe_file_name = global_log_filename;
  std::replace(safe_file_name.begin(), safe_file_name.end(), '/', '_');
  std::replace(safe_file_name.begin(), safe_file_name.end(), '\\', '_');
  return safe_file_name;
}

int _CaptureStdoutToFile(const std::string& filename) {
  if (++_is_capturing > 1) {
    std::cerr << "CaptureStdoutToFile - Error: Already capturing stdout!\n";
    _is_capturing--;
    return -1;
  }

  std::fflush(stdout);
  std::cout.flush();

  int fd = os_open(filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
  if (fd == -1) {
    std::cerr << "CaptureStdoutToFile - Error: open('" << filename
              << "') failed: " << GetErrStr() << "\n";
    _is_capturing--;
    return -2;
  }

  _saved_stdout_fd = os_dup(os_fileno_stdout());
  if (_saved_stdout_fd == -1) {
    std::cerr
        << "CaptureStdoutToFile - Error: os_dup(os_fileno_stdout()) failed: "
        << GetErrStr() << "\n";
    os_close(fd);
    _is_capturing--;
    return -3;
  }

  if (os_dup2(fd, os_fileno_stdout()) == -1) {
    std::cerr << "CaptureStdoutToFile - Error: os_dup2(fd, os_fileno_stdout()) "
                 "failed: "
              << GetErrStr() << "\n";
    os_close(fd);
    os_close(_saved_stdout_fd);
    _saved_stdout_fd = -1;
    _is_capturing--;
    return -4;
  }

  os_close(fd);
  std::ios::sync_with_stdio(true);
  std::fflush(stdout);
  std::cout.flush();
  return 0;
}

int _RestoreCaptureStdout() {
  if (_is_capturing == 0) {
    std::cerr << "RestoreCaptureStdout - Error: Not capturing stdout!\n";
    return -1;
  }

  std::fflush(stdout);
  std::cout.flush();

  if (_saved_stdout_fd == -1) {
    std::cerr << "RestoreCaptureStdout - Error: Saved fd invalid\n";
    return -2;
  }

  if (os_dup2(_saved_stdout_fd, os_fileno_stdout()) == -1) {
    std::cerr << "RestoreCaptureStdout - Error: os_dup2(_saved_stdout_fd, "
                 "os_fileno_stdout()) "
                 "failed: "
              << GetErrStr() << "\n";
    os_close(_saved_stdout_fd);
    _saved_stdout_fd = -1;
    _is_capturing--;
    return -3;
  }

  os_close(_saved_stdout_fd);
  _saved_stdout_fd = -1;
  _is_capturing--;

  std::fflush(stdout);
  std::cout.flush();
  std::ios::sync_with_stdio(true);

  return 0;
}

int CaptureStdoutToPipe() {
  if (++_is_capturing > 1) {
    std::cerr << "_CaptureStdoutToString - Error: Already capturing stdout!\n";
    _is_capturing--;
    return -1;
  }

  std::fflush(stdout);
  std::cout.flush();

  int pipefd[2];

  if (os_pipe(pipefd) == -1) {
    std::cerr << "_CaptureStdoutToStringStart - Error: os_pipe() failed: "
              << GetErrStr() << "\n";
    _is_capturing--;
    return -2;
  }

  _saved_pipe_read_fd = pipefd[0];
  int write_fd = pipefd[1];

  _saved_stdout_fd = os_dup(os_fileno_stdout());
  if (_saved_stdout_fd == -1) {
    std::cerr << "_CaptureStdoutToStringStart - Error: "
                 "os_dup(os_fileno_stdout()) failed: "
              << GetErrStr() << "\n";
    os_close(_saved_pipe_read_fd);
    _saved_pipe_read_fd = -1;
    os_close(write_fd);
    _is_capturing--;
    return -3;
  }

  if (os_dup2(write_fd, os_fileno_stdout()) == -1) {
    std::cerr << "CaptureStdoutToFile - Error: os_dup2(fd, os_fileno_stdout()) "
                 "failed: "
              << GetErrStr() << "\n";
    os_close(_saved_pipe_read_fd);
    _saved_pipe_read_fd = -1;
    os_close(write_fd);
    os_close(_saved_stdout_fd);
    _saved_stdout_fd = -1;
    _is_capturing--;
    return -4;
  }

  os_close(write_fd);
  std::ios::sync_with_stdio(true);
  std::fflush(stdout);
  std::cout.flush();
  return 0;
}

int RestoreCaptureStdoutToStr(std::string& out) {
  if (_is_capturing == 0) {
    std::cerr << "RestoreCaptureStdoutToStr - Error: Not capturing stdout!\n";
    return -1;
  }
  if (_saved_stdout_fd == -1) {
    std::cerr
        << "RestoreCaptureStdoutToStr - Error: Saved stdout fd invalid \n";
    return -2;
  }

  std::fflush(stdout);
  std::cout.flush();

  if (os_dup2(_saved_stdout_fd, os_fileno_stdout()) == -1) {
    std::cerr << "RestoreCaptureStdoutToStr - Error: os_dup2(_saved_stdout_fd, "
                 "os_fileno_stdout()) failed: "
              << GetErrStr() << "\n";
    return -3;
  }

  os_close(_saved_stdout_fd);
  _saved_stdout_fd = -1;

  if (_saved_pipe_read_fd == -1) {
    std::cout
        << "RestoreCaptureStdoutToStr - Warning: Nothing to read in pipe \n";
    out.clear();
    return 1;
  }

  out.clear();
  constexpr size_t kBufSize = 4096;
  char buffer[kBufSize];

  while (true) {
    SSIZE_T n = os_read(_saved_pipe_read_fd, buffer, kBufSize);
    if (n > 0) {
      out.append(buffer, static_cast<size_t>(n));
    } else if (n == 0) {
      // EOF
      break;
    } else {
      if (errno == EINTR) continue;
      std::cerr << "RestoreCaptureStdoutToStr - Error: os_read() failed: "
                << GetErrStr() << "\n";
      os_close(_saved_pipe_read_fd);
      _saved_pipe_read_fd = -1;
      return -4;
    }
  }

  os_close(_saved_pipe_read_fd);
  _saved_pipe_read_fd = -1;

  std::fflush(stdout);
  std::cout.flush();
  std::ios::sync_with_stdio();

  _is_capturing--;

  return 0;
}

int RedirectCreateStatusbarHandle(
    statusbar_log::StatusbarHandle& statusbar_handle,
    const statusbar_log::sink::SinkHandle& sink_handle,
    const std::vector<unsigned int> _positions,
    const std::vector<unsigned int> _bar_sizes,
    const std::vector<std::string> _prefixes,
    const std::vector<std::string> _postfixes,
    const std::string& log_filename) {
  _CaptureStdoutToFile(log_filename);
  int err_code = statusbar_log::CreateStatusbarHandle(
      statusbar_handle, sink_handle, _positions, _bar_sizes, _prefixes,
      _postfixes);
  _RestoreCaptureStdout();
  return err_code;
}

int RedirectDestroyStatusbarHandle(
    statusbar_log::StatusbarHandle& statusbar_handle,
    const std::string& log_filename) {
  _CaptureStdoutToFile(log_filename);
  int err_code = statusbar_log::DestroyStatusbarHandle(statusbar_handle);
  _RestoreCaptureStdout();
  return err_code;
}

int RedirectUpdateStatusbar(statusbar_log::StatusbarHandle& statusbar_handle,
                            const std::size_t idx, const double percent,
                            const std::string& log_filename) {
  _CaptureStdoutToFile(log_filename);
  int err_code = statusbar_log::UpdateStatusbar(statusbar_handle, idx, percent);
  _RestoreCaptureStdout();
  return err_code;
}

int RedirectToStrUpdateStatusbar(
    std::string& capture_stdout,
    statusbar_log::StatusbarHandle& statusbar_handle, const std::size_t idx,
    const double percent) {
  CaptureStdoutToPipe();
  int err_code = statusbar_log::UpdateStatusbar(statusbar_handle, idx, percent);
  RestoreCaptureStdoutToStr(capture_stdout);
  return err_code;
}

int RedirectToStrLog(std::string& capture_stdout,
                     const statusbar_log::sink::SinkHandle& sink_handle,
                     LogLevel log_level, const std::string filename,
                     const char* fmt, ...) {
  CaptureStdoutToPipe();
  va_list args;
  va_start(args, fmt);
  int err_code = LogV(log_level, filename, 0, sink_handle, fmt, args);
  va_end(args);
  RestoreCaptureStdoutToStr(capture_stdout);
  std::string capture_stdout_cleaned = StripAnsiEscapeSequences(capture_stdout);
  capture_stdout = capture_stdout_cleaned;
  return err_code;
}

std::string StripAnsiEscapeSequences(const std::string& s) {
  std::string esc = "\x1B";
  std::string pattern = esc + R"(\[[0-?]*[ -/]*[@-~])";
  std::string cleaned = std::regex_replace(s, std::regex(pattern), "");
  std::string out;
  out.reserve(cleaned.size());
  for (char c : cleaned) {
    if (c == '\n' || static_cast<unsigned char>(c) >= 32) {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace test
}  // namespace statusbar_log
