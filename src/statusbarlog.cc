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

// -- statusbarlog/src/statusbarlog.cc

// clang-format off

#include "statusbarlog/statusbarlog.h"
#include <climits>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

#include "statusbarlog/sink.h"

// clang-format on

const std::string kFilename = "statusbarlog.cc";

namespace statusbar_log {

// Hidden implementation detail
namespace {

/**
 * \struct Statusbar
 * \brief Represents a multi-component status bar with progress indicators.
 *
 * A status bar can contain multiple stacked bars, each with:
 * - Progress percentages (0-100) for each bar.
 * - Vertical positions (1=topmost).
 * - Total width (characters) of each bar.
 * - Text displayed before each bar.
 * - Text displayed after each bar.
 * - Spinner animation indices.
 * - Indicator whether error already has been reported
 * - unique id corresponding to the handle
 */
// clang-format off
typedef struct {
  sink::SinkHandle sink_handle;         ///< The sink in which to print the statusbar (for e.g. stdout).
  std::vector<double> percentages;      ///< Progress percentages (0-100) for each bar.
  std::vector<unsigned int> positions;  ///< Vertical positions (1=topmost).
  std::vector<unsigned int> bar_sizes;  ///< Total width (characters) of each bar.
  std::vector<std::string> prefixes;    ///< Text displayed before each bar.
  std::vector<std::string> postfixes;   ///< Text displayed after each bar.
  std::vector<std::size_t> spin_idxs;   ///< Spinner animation indices.
  unsigned int id;                      ///< unique id corresponding to the handle
  bool error_reported;                  ///< Indicator whether error already has been reported
} Statusbar;
// clang-format on

/**
 *
 */
std::vector<Statusbar> _statusbar_registry = {};
std::vector<StatusbarHandle> _statusbar_free_handles = {};
unsigned int _statusbar_handle_id_count = 0;

static std::mutex _statusbar_registry_mutex;
static std::mutex _statusbar_id_count_mutex;

/**
 * \brief Conditionally flushes the output based on
 * statusbar_log::kStatusbarLogNoAutoFlush setting
 */
void _ConditionalFlush(sink::SinkHandle sink_handle) {
  if (!kStatusbarLogNoAutoFlush) {
    sink::FlushSinkHandle(sink_handle);
  }
}

/**
 * \brief Gets terminal width in columns.
 *
 * \param[out] width Receives terminal width. Defaults to 80 on failure.
 *
 * \return statusbar_log::kStatusbarLogSuccess (i.e. 0) on success
 * \return -1 (Windows) or -2 (Unix) on failure
 *
 */
int _GetTerminalWidth(int& width) {
  width = 80;  // Default value

#ifdef _WIN32
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConsole != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
      width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    } else {
      return -1;
    }
  }
#else
  winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    width = w.ws_col;
  } else {
    return -2;
  }
#endif
  return kStatusbarLogSuccess;
}

/**
 * \brief Sanitize string for use in log function or post- and prefixes
 * in statusbars
 *
 * \brief This functions replaces all control charachters except \n and \t
 * of an input string.
 */
std::string _SanitizeStringWithNewline(const std::string& input) {
  std::string output;
  output.reserve(input.size());
  for (char c : input) {
    // Allow for '\n' and '\t' charachters
    if (c == '\n' || c == '\t' || (c >= 32 && c <= 126)) {
      output += c;
    } else if (static_cast<unsigned char>(c) < 32 || c == 127) {
      output += "�";
    } else {
      output += c;
    }
  }
  return output;
}

/**
 * \brief Sanitize string for use in log function or post- and prefixes
 * in statusbars
 *
 * \brief This functions replaces all control charachters except \t
 * of an input string.
 */
std::string _SanitizeString(const std::string& input) {
  std::string output;
  output.reserve(input.size());
  for (char c : input) {
    // Allow for '\n' and '\t' charachters
    if (c == '\t' || (c >= 32 && c <= 126)) {
      output += c;
    } else if (static_cast<unsigned char>(c) < 32 || c == 127) {
      output += "�";
    } else {
      output += c;
    }
  }
  return output;
}

/**
 * \brief Check if the argument is a valid statusbar handle
 *
 * This functions performs a test on a StatusbarHandle and returns
 * statusbar_log::kStatusbarLogSuccess (i.e. 0) if it is a valid handle and a
 * negative number otherwise
 *
 * \param[in] StatusbarHandle struct to be checked for validity
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the handle is
 * valid, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Invalid handle: Valid flag of handle set to false
 *         - -2: Invalid handle: Handle index out of bounds in
 * `statusbar_registry`
 *         - -3: Invalid handle: Handle IDs don't match between handle struct
 *         - -4: Invalid handle: Handle ID is 0 (i.e. invalid)
 * and registry
 *
 * \see _IsValidHandleVerbose: Verbose version of this function
 */
int _IsValidStatusbarHandle(const StatusbarHandle& statusbar_handle) {
  const std::size_t idx = statusbar_handle.idx;

  if (!statusbar_handle.valid) {
    return -1;
  }
  if ((statusbar_handle.idx >= _statusbar_registry.size()) ||
      (statusbar_handle.idx == SIZE_MAX)) {
    return -2;
  }
  if (statusbar_handle.id != _statusbar_registry[idx].id) {
    return -3;
  }
  if (statusbar_handle.id == 0) {
    return -4;
  }
  return kStatusbarLogSuccess;
}

/**
 * \brief Check if the argument is a valid statusbar handle and prints an error
 * message.
 *
 * This functions performs a test on a StatusbarHandle and returns
 * statusbar_log::kStatusbarLogSuccess (i.e. 0) if it is a valid handle and a
 * negative number otherwise. Same as _IsValidHandle but also prints an error
 * message.
 *
 * \param[in] StatusbarHandle struct to be checked for validity
 * \param[in] SinkHandle Struct which consumes all error-messages generated by
 * this function.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the handle is
 * valid, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Invalid handle: Valid flag of handle set to false
 *         - -2: Invalid handle: Handle index out of bounds in
 * `statusbar_registry`
 *         - -3: Invalid handle: Handle IDs don't match between handle struct
 *         - -4: Invalid handle: Handle ID is 0 (i.e. invalid)
 *         - -5: Invalid handle: Errorcode not handled
 * and registry
 *
 * \see _IsValidHandle: Non verbose version of this function
 */
int _IsValidStatusbarHandleVerbose(const StatusbarHandle& statusbar_handle,
                                   const sink::SinkHandle& err_sink_handle) {
  const int is_valid_handle = _IsValidStatusbarHandle(statusbar_handle);
  if (is_valid_handle == -1) {
    LogWrn(kFilename, err_sink_handle,
           "Invalid handle: Valid flag set to false (idx: %zu, ID: %u)",
           statusbar_handle.idx, statusbar_handle.id);
    return -1;
  }

  if (is_valid_handle == -2) {
    LogWrn(kFilename, err_sink_handle,
           "Invalid Handle: Handle index %zu out of bounds (max %zu)",
           statusbar_handle.idx, _statusbar_registry.size());
    return -2;
  }

  Statusbar& target = _statusbar_registry[statusbar_handle.idx];

  if (is_valid_handle == -3) {
    LogWrn(kFilename, err_sink_handle,
           "Invalid Handle: ID mismatch: handle %u vs registry %u",
           statusbar_handle.id, target.id);
    return -3;
  }

  if (is_valid_handle == -4) {
    LogWrn(kFilename, err_sink_handle,
           "Invalid Handle: ID is 0 (i.e. invalid)");
    return -4;
  }

  if (is_valid_handle != kStatusbarLogSuccess) {
    LogWrn(kFilename, err_sink_handle,
           "Invalid Handle: Errorcode not handled!");
    return -5;
  }

  return kStatusbarLogSuccess;
}

/**
 * \brief Function used only by that StatusbarLog module to draw a single status
 * bar at a certain position.
 *
 * This function draws a single status bar (not multiple stacked ones) from
 * primitive variables.
 *
 * Example single statusbar:
 * "prefix string"[########/       ] 50% "postfix string"
 *
 * the bar can be drawn at an arbitrary postion above or on the cursur using the
 * `move` parameter.
 *
 * \param[in] SinkHandle Struct to use for writing.
 * \param[in] write_lock Unique lock of the sink to write to (should already be
 * locked).
 * \param[in] percent: Progress percentage (0-100).
 * \param[in] bar_width: Total bar width (characters excluding prefix, postfix,
 * percentage, '[' and '[').
 * \param[in] prefix: Text before the bar.
 * \param[in] postfix: Text after the bar.
 * \param[in, out] spinner_idx: Index for spinner animation (incremented on
 * call).
 * \param[in] move: Vertical offset from cursor (positive = up).
 *
 * \details Using the spinner_idx the spinner character can cycle through { |,
 * /, -, \ } on each update.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -1: Terminal width detection failed (Windows)
 *         - -2: Terminal width detection failed (Linux)
 *         - -3: Truncantion was needed (bar exeeds terminal width)
 *         - -4: Both terminal width detection failed (Window) AND truncation
 * was needed
 *         - -5: Both terminal width detection failed (Linux) AND truncation was
 *         - -6: Invalid percentage given
 *         - -7: write_lock not owned (cannot print without write_lock owned)
 *         - -8: Sink write failed.
 * needed
 */
int _DrawStatusbarComponent(const sink::SinkHandle& sink_handle,
                            std::unique_lock<std::mutex>& write_lock,
                            const double percent, const unsigned int bar_width,
                            const std::string& prefix,
                            const std::string& postfix,
                            std::size_t& spinner_idx, const int move) {
  if (!write_lock.owns_lock()) {
    return -7;
  }
  if (percent > 100.0 || percent < 0.0) {
    write_lock.unlock();

    LogErr(kFilename, sink_handle,
           "Failed to update statusbar: Invalid percentage.");
    write_lock.lock();
    return -5;
  }

  int err = kStatusbarLogSuccess;
  static const std::array<char, 4> spinner = {'|', '/', '-', '\\'};
  spinner_idx %= spinner.size();
  char spin_char = spinner[spinner_idx];

  const unsigned int fill = static_cast<unsigned int>(
      std::floor((percent * static_cast<double>(bar_width)) / 100.0));
  const unsigned int empty = bar_width - fill;

  std::ostringstream oss;
  oss << prefix;
  oss << "[";
  oss << std::string(fill, '#');
  if (empty > 0) {
    oss << spin_char;
    oss << std::string(empty - 1, ' ');
  } else {
    oss << std::string(empty, ' ');
  }
  oss << "] ";
  oss << std::fixed << std::setprecision(2) << std::setw(6) << percent;
  oss << postfix;
  std::string status_str = oss.str();

  int term_width;
  if (sink::SinkIsTty(sink_handle)) {
    err = _GetTerminalWidth(term_width);
  } else {
    term_width = INT_MAX;
  }

  if (status_str.length() > static_cast<size_t>(term_width)) {
    status_str = status_str.substr(0, static_cast<size_t>(term_width - 1));
    switch (err) {
      case kStatusbarLogSuccess:
        err = -3;
        break;
      case -1:
        err = -4;
        break;
      case -2:
        err = -5;
        break;
    }
  }

  sink::MoveCursorUp(sink_handle, move);
  ClearCurrentLine(sink_handle);
  SSIZE_T written = sink::SinkWriteStr(sink_handle, status_str);
  if (written <= 0) {
    std::cout << "ERROR [" << kFilename << "]: "
              << "Sink Write Failed in _DrawStatusbarComponent!\n";
    return -8;
  }
  _ConditionalFlush(sink_handle);
  sink::MoveCursorUp(sink_handle, -move);

  return err;
}

/**
 * \brief Write "prefix ...\n" to a file sink when a statusbar starts.
 *
 * For file sinks, statusbars are not drawn. Instead, the prefix of the
 * topmost bar (index 0) is written with " ...\n" appended.
 *
 * \param[in] sink_handle Sink handle to write to (must be a file sink).
 * \param[in] statusbar The statusbar whose topmost prefix to write.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error codes:
 *         - -1 to -4: Invalid sink handle (forwarded from get_sink_type_silent)
 *         - -5: Statusbar has no bars.
 *         - -6: Sink is not a file sink.
 *         - -7: Sink write failed.
 */
int _WriteFileStatusbarPrefix(const sink::SinkHandle& sink_handle,
                              const Statusbar& statusbar) {
  if (statusbar.prefixes.empty()) return -5;

  sink::SinkType sink_type;
  int err = sink::get_sink_type_silent(sink_handle, sink_type);
  if (err != kStatusbarLogSuccess) return err;
  if (sink_type != sink::kSinkFileOwned) return -6;

  SSIZE_T written =
      sink::SinkWriteStr(sink_handle, statusbar.prefixes[0] + " ...\n");
  if (written <= 0) return -7;

  _ConditionalFlush(sink_handle);
  return kStatusbarLogSuccess;
}

/**
 * \brief Write "prefix Done!\n" to a file sink when a statusbar completes.
 *
 * \param[in] sink_handle Sink handle to write to (must be a file sink).
 * \param[in] statusbar The statusbar whose topmost prefix to use.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error codes:
 *         - -1 to -4: Invalid sink handle (forwarded from get_sink_type_silent)
 *         - -5: Statusbar has no bars.
 *         - -6: Sink is not a file sink.
 *         - -7: Sink write failed.
 */
int _WriteFileStatusbarDone(const sink::SinkHandle& sink_handle,
                            const Statusbar& statusbar) {
  if (statusbar.prefixes.empty()) return -5;

  sink::SinkType sink_type;
  int err = sink::get_sink_type_silent(sink_handle, sink_type);
  if (err != kStatusbarLogSuccess) return err;
  if (sink_type != sink::kSinkFileOwned) return -6;

  SSIZE_T written =
      sink::SinkWriteStr(sink_handle, statusbar.prefixes[0] + " Done!\n");
  if (written <= 0) return -7;

  _ConditionalFlush(sink_handle);
  return kStatusbarLogSuccess;
}

}  // namespace

void SaveCursorPosition(sink::SinkHandle sink_handle) {
  std::cout << "\033[s";  // ANSI escape code to save cursor position
  _ConditionalFlush(sink_handle);
}

void RestoreCursorPosition(sink::SinkHandle sink_handle) {
  std::cout << "\033[u";  // ANSI escape code to restore cursor position
  _ConditionalFlush(sink_handle);
}

void ClearToEndOfLine(sink::SinkHandle sink_handle) {
  std::cout << "\033[0K";  // ANSI escape code to clear to end of line
  _ConditionalFlush(sink_handle);
}

void ClearFromStartOfLine(sink::SinkHandle sink_handle) {
  std::cout << "\033[1K";  // ANSI escape code to clear to end of line
  _ConditionalFlush(sink_handle);
}

void ClearLine(sink::SinkHandle sink_handle) {
  std::cout << "\033[2K";
  _ConditionalFlush(sink_handle);
}

void ClearCurrentLine(sink::SinkHandle sink_handle) {
  std::cout << "\r"        // Return to line start
            << "\033[2K";  // Clear entire line
  _ConditionalFlush(sink_handle);
}

int LogV(const LogLevel log_level, const std::string& filename,
         sink::SinkHandle sink_handle, const char* fmt, va_list args) {
  if (log_level > kLogLevel) return kStatusbarLogSuccess;
  std::mutex* write_mutex_ptr = nullptr;
  int err = sink::get_mutex_ptr(sink_handle, write_mutex_ptr);

  sink::SinkType sink_type;
  err = sink::get_sink_type(sink_handle, sink_type);
  if (err != kStatusbarLogSuccess) return err;

  std::unique_lock<std::mutex> write_lock(*write_mutex_ptr, std::defer_lock);
  std::unique_lock<std::mutex> registry_lock(_statusbar_registry_mutex,
                                             std::defer_lock);
  std::lock(write_lock, registry_lock);

  const char* prefix = "";
  // clang-format off
  switch(log_level){
    case kLogLevelErr: prefix = "ERROR"; break;
    case kLogLevelWrn: prefix = "WARNING"; break;
    case kLogLevelInf: prefix = "INFO"; break;
    case kLogLevelDbg: prefix = "DEBUG"; break;
    default: break;
  }
  // clang-format on

  std::vector<std::size_t> relevant_statusbar_idxs = {};
  for (std::size_t i = 0; i < _statusbar_registry.size(); ++i) {
    if (_statusbar_registry[i].id == 0) continue;
    sink::SinkType statusbar_sink_type;
    err = sink::get_sink_type_silent(_statusbar_registry[i].sink_handle,
                                     statusbar_sink_type);
    if (err != kStatusbarLogSuccess) continue;
    if (statusbar_sink_type != sink_type) continue;
    relevant_statusbar_idxs.push_back(i);
  }

  int move = 0;
  for (std::size_t i : relevant_statusbar_idxs) {
    for (std::size_t j = 0; j < _statusbar_registry[i].positions.size(); ++j) {
      int current_pos = static_cast<int>(_statusbar_registry[i].positions[j]);
      if (current_pos > move) {
        move = current_pos;
      }
    }
  }

  va_list args_copy;
  va_copy(args_copy, args);
  int size = std::vsnprintf(nullptr, 0, fmt, args_copy);
  va_end(args_copy);
  size = std::min(size, static_cast<int>(kMaxLogLength));
  std::vector<char> buffer(static_cast<unsigned int>(size) + 1);
  va_copy(args_copy, args);
  std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
  va_end(args_copy);
  std::string message = _SanitizeStringWithNewline(buffer.data());

  std::string sanitized_filename = _SanitizeStringWithNewline(filename);
  if (sanitized_filename.length() > kMaxFilenameLength) {
    sanitized_filename.resize(kMaxFilenameLength - 3);
    sanitized_filename += "...";
  }

  std::string formatted_message =
      std::string(prefix) + " [" + sanitized_filename + "]: " + message + "\n";

  if (sink_type == sink::kSinkFileOwned) {
    SSIZE_T written = sink::SinkWriteStr(sink_handle, formatted_message);
    if (written <= 0) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Sink Write Failed in LogV!\n";
      return -6;
    }
    _ConditionalFlush(sink_handle);
    write_lock.unlock();
    registry_lock.unlock();
    return kStatusbarLogSuccess;
  }

  sink::MoveCursorUp(sink_handle, move);
  if (!relevant_statusbar_idxs.empty()) printf("\r\033[2K\r");

  SSIZE_T written = sink::SinkWriteStr(sink_handle, formatted_message);
  if (written <= 0) {
    std::cout << "ERROR [" << kFilename << "]: "
              << "Sink Write Failed in LogV!\n";
    return -6;
  }

  _ConditionalFlush(sink_handle);
  sink::MoveCursorUp(sink_handle, -move);

  for (std::size_t i : relevant_statusbar_idxs) {
    for (std::size_t j = 0; j < _statusbar_registry[i].positions.size(); ++j) {
      int bar_err_code = _DrawStatusbarComponent(
          sink_handle, write_lock, _statusbar_registry[i].percentages[j],
          _statusbar_registry[i].bar_sizes[j],
          _statusbar_registry[i].prefixes[j],
          _statusbar_registry[i].postfixes[j],
          _statusbar_registry[i].spin_idxs[j],
          static_cast<int>(_statusbar_registry[i].positions[j]));
      if ((bar_err_code != kStatusbarLogSuccess) &&
          !_statusbar_registry[i].error_reported) {
        std::string why;
        bool is_critical_error = false;
        switch (bar_err_code) {
          case -1:
            is_critical_error = true;
            why = "Terminal width detection failed (Windows)";
            break;
          case -2:
            is_critical_error = true;
            why = "Terminal width detection failed (Linux)";
            break;
          case -3:
            is_critical_error = false;
            why = "Truncantion was needed (bar exeeds terminal width)";
            break;
          case -4:
            is_critical_error = true;
            why =
                "Both terminal width detection failed (Window) AND "
                "truncation";
            break;
          case -5:
            is_critical_error = true;
            why = "Both terminal width detection failed (Linux) AND truncation";
            break;
          case -6:
            is_critical_error = true;
            why = "Invalid percentage given";
            break;
          default:
            is_critical_error = true;
            why = "Unknown _DrawStatusbarComponent error!";
            break;
        }
        if (is_critical_error) {
          _statusbar_registry[i].error_reported = true;
          write_lock.unlock();
          registry_lock.unlock();
          printf(
              "ERROR [statusbarlog.cc]: LogV(...) failed updating "
              "statusbar: %s on statusbar with ID %zu at bar idx %zu",
              why.c_str(), i, j);
          return bar_err_code - 5;
        }
      }
    }
  }

  write_lock.unlock();
  registry_lock.unlock();
  return kStatusbarLogSuccess;
}

int CreateStatusbarHandle(StatusbarHandle& statusbar_handle,
                          const sink::SinkHandle sink_handle,
                          const std::vector<unsigned int> _positions,
                          const std::vector<unsigned int> _bar_sizes,
                          const std::vector<std::string> _prefixes,
                          const std::vector<std::string> _postfixes) {
  int err = sink::IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) {
    std::cout << "ERROR [" << kFilename << "]: "
              << "Failed to create Statusbar Handle! Sink Handle is invalid";
    return -1;
  }

  err = _IsValidStatusbarHandle(statusbar_handle);
  if (err == kStatusbarLogSuccess) {
    LogErr(
        kFilename, sink_handle,
        "Failed to create Statusbar Handle! Handle id matches already active "
        "statusbar. (_IsValidHandle returned %d)",
        err);
    return -2;
  }
  statusbar_handle.valid = false;
  statusbar_handle.id = 0;
  if (_positions.size() != _bar_sizes.size() ||
      _bar_sizes.size() != _prefixes.size() ||
      _prefixes.size() != _postfixes.size()) {
    LogErr(kFilename, sink_handle,
           "Failed to create statusbar_handle The vecotors '_positions', "
           "'_bar_sizes', '_prefixes' and "
           "'_postfixes' must have the same size! Got: '_positions': %zu, "
           "'_bar_sizes': %zu, '_prefixes': %zu, '_postfixes': %zu.",
           _positions.size(), _bar_sizes.size(), _prefixes.size(),
           _postfixes.size());
    return -3;
  }

  if (_statusbar_registry.size() - _statusbar_free_handles.size() >=
      kMaxStatusbarHandles) {
    LogErr(kFilename, sink_handle,
           "Failed to create statusbar handle. Maximum number of status bars "
           "(%zu) reached",
           kMaxStatusbarHandles);
    return -4;
  }

  std::mutex* write_mutex_ptr;
  err = sink::get_mutex_ptr(sink_handle, write_mutex_ptr);
  if (err != kStatusbarLogSuccess) {
    LogErr(kFilename, sink_handle,
           "Failed to destory statusbar_handle! Failed to get sink mutex ptr. "
           "Errorcode: %d",
           err);
    return -5;
  }
  std::unique_lock<std::mutex> write_lock(*write_mutex_ptr, std::defer_lock);
  std::unique_lock<std::mutex> registry_lock(_statusbar_registry_mutex,
                                             std::defer_lock);
  std::unique_lock<std::mutex> id_count_lock(_statusbar_id_count_mutex,
                                             std::defer_lock);
  std::lock(write_lock, registry_lock, id_count_lock);

  _statusbar_handle_id_count++;
  if (_statusbar_handle_id_count == 0) {
    write_lock.unlock();
    registry_lock.unlock();
    LogWrn(kFilename, sink_handle,
           "Max number of possible statusbar handle ids reached, looping back "
           "to 1");
    std::lock(write_lock, registry_lock);
    _statusbar_handle_id_count++;
  }

  const std::size_t num_bars = _positions.size();
  const std::vector<double> percentages(num_bars, 0.0);
  const std::vector<std::size_t> spin_idxs(num_bars, 0);

  std::vector<std::string> sanitized_prefixes;
  sanitized_prefixes.reserve(_prefixes.size());
  std::vector<std::string> sanitized_postfixes;
  sanitized_postfixes.reserve(_postfixes.size());
  std::vector<unsigned int> sanitized_bar_sizes;
  sanitized_bar_sizes.reserve(_bar_sizes.size());
  for (std::size_t i = 0; i < _prefixes.size(); ++i) {
    std::string _prefix = _prefixes[i];
    if (_prefix.length() > kMaxPrefixLength) {
      _prefix.resize(kMaxPrefixLength - 3);
      _prefix += "...";
    }
    sanitized_prefixes.push_back(_SanitizeString(_prefix));

    std::string _postfix = _postfixes[i];
    if (_postfix.length() > kMaxPostfixLength) {
      _postfix.resize(kMaxPostfixLength - 3);
      _postfix += "...";
    }
    sanitized_postfixes.push_back(_SanitizeString(_postfix));

    sanitized_bar_sizes.push_back(
        std::min<unsigned int>(_bar_sizes[i], kMaxBarWidth));
  }

  if (!_statusbar_free_handles.empty()) {
    StatusbarHandle free_handle = _statusbar_free_handles.back();
    _statusbar_free_handles.pop_back();
    statusbar_handle.idx = free_handle.idx;
    _statusbar_registry[statusbar_handle.idx] = {sink_handle,
                                                 percentages,
                                                 _positions,
                                                 sanitized_bar_sizes,
                                                 sanitized_prefixes,
                                                 sanitized_postfixes,
                                                 spin_idxs,
                                                 _statusbar_handle_id_count,
                                                 false};
  } else {
    statusbar_handle.idx = _statusbar_registry.size();
    _statusbar_registry.emplace_back(
        Statusbar{sink_handle, percentages, _positions, sanitized_bar_sizes,
                  sanitized_prefixes, sanitized_postfixes, spin_idxs,
                  _statusbar_handle_id_count, false});
  }

  statusbar_handle.id = _statusbar_handle_id_count;
  statusbar_handle.valid = true;

  sink::SinkType sink_type;
  sink::get_sink_type_silent(sink_handle, sink_type);
  if (sink_type == sink::kSinkFileOwned) {
    _WriteFileStatusbarPrefix(sink_handle,
                              _statusbar_registry[statusbar_handle.idx]);
  } else {
    for (std::size_t idx = 0; idx < num_bars; idx++) {
      _DrawStatusbarComponent(
          sink_handle, write_lock, 0.0,
          _statusbar_registry[statusbar_handle.idx].bar_sizes[idx],
          _statusbar_registry[statusbar_handle.idx].prefixes[idx],
          _statusbar_registry[statusbar_handle.idx].postfixes[idx],
          _statusbar_registry[statusbar_handle.idx].spin_idxs[idx],
          static_cast<int>(
              _statusbar_registry[statusbar_handle.idx].positions[idx]));
    }
  }
  return kStatusbarLogSuccess;
}

int DestroyStatusbarHandle(StatusbarHandle& statusbar_handle) {
  int err = _IsValidStatusbarHandle(statusbar_handle);
  if (err != kStatusbarLogSuccess) {
    std::cout
        << "ERROR [" << kFilename
        << "]: Failed to destory statusbar_handle! Invalid statusbar_handle:"
           "_IsValidStatusbarHandle error code: "
        << err << "\n";
    return err;
  }

  sink::SinkHandle& sink_handle =
      _statusbar_registry[statusbar_handle.idx].sink_handle;
  err = sink::IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) {
    std::cout << "ERROR [" << kFilename
              << "]: Failed to destory statusbar_handle! Invaild sink handle "
                 "in statusbar:"
                 "IsValidSinkHandle error code: "
              << err << "\n";
    return -5;
  }

  std::mutex* write_mutex_ptr;
  err = sink::get_mutex_ptr(sink_handle, write_mutex_ptr);
  if (err != kStatusbarLogSuccess) {
    LogErr(kFilename, sink_handle,
           "Failed to destory statusbar_handle! Failed to get sink mutex ptr. "
           "Errorcode: %d",
           err);
    return -6;
  }
  std::unique_lock<std::mutex> write_lock(*write_mutex_ptr, std::defer_lock);
  std::unique_lock<std::mutex> registry_lock(_statusbar_registry_mutex,
                                             std::defer_lock);
  std::lock(write_lock, registry_lock);

  Statusbar& target = _statusbar_registry[statusbar_handle.idx];

  sink::SinkType sink_type;
  sink::get_sink_type_silent(sink_handle, sink_type);
  if (sink_type == sink::kSinkFileOwned) {
    _WriteFileStatusbarDone(sink_handle, target);
  } else {
    for (std::size_t i = 0; i < target.positions.size(); i++) {
      sink::MoveCursorUp(sink_handle, static_cast<int>(target.positions[i]));
      ClearCurrentLine(sink_handle);
      sink::MoveCursorUp(sink_handle, -static_cast<int>(target.positions[i]));
    }
  }
  sink::FlushSinkHandle(sink_handle);

  target.sink_handle = sink::SinkHandle();
  target.percentages.clear();
  target.positions.clear();
  target.bar_sizes.clear();

  for (std::string& str : target.prefixes) {
    std::fill(str.begin(), str.end(), '\0');
  }
  for (std::string& str : target.postfixes) {
    std::fill(str.begin(), str.end(), '\0');
  }
  target.prefixes.clear();
  target.postfixes.clear();

  target.id = 0;
  target.spin_idxs.clear();

  statusbar_handle.valid = false;
  statusbar_handle.id = 0;
  _statusbar_free_handles.push_back(statusbar_handle);

  write_lock.unlock();
  registry_lock.unlock();
  return kStatusbarLogSuccess;
}

int UpdateStatusbar(StatusbarHandle& statusbar_handle, const std::size_t idx,
                    const double percent) {
  sink::SinkHandle& sink_handle =
      _statusbar_registry[statusbar_handle.idx].sink_handle;
  int err = sink::IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) {
    std::cout << "ERROR [" << kFilename
              << "]: Failed to update statusbar! Invaild sink handle "
                 "in statusbar:"
                 "IsValidSinkHandle error code: "
              << err << "\n";
    return -1;
  }

  sink::SinkType sink_type;
  err = sink::get_sink_type_silent(sink_handle, sink_type);
  if (err == kStatusbarLogSuccess && sink_type == sink::kSinkFileOwned) {
    return kStatusbarLogSuccess;
  }

  std::mutex* write_mutex_ptr;
  err = sink::get_mutex_ptr(sink_handle, write_mutex_ptr);
  if (err != kStatusbarLogSuccess) {
    LogErr(kFilename, sink_handle,
           "Failed to update statusbar! Failed to get sink mutex ptr. "
           "Errorcode: %d",
           err);
    return -2;
  }

  std::unique_lock<std::mutex> write_lock(*write_mutex_ptr, std::defer_lock);
  std::unique_lock<std::mutex> registry_lock(_statusbar_registry_mutex,
                                             std::defer_lock);
  std::lock(write_lock, registry_lock);

  err = _IsValidStatusbarHandle(statusbar_handle);
  if (err != kStatusbarLogSuccess) {
    write_lock.unlock();
    registry_lock.unlock();
    _IsValidStatusbarHandleVerbose(statusbar_handle, sink_handle);
    LogErr(kFilename, sink_handle,
           "Failed to update statusbar: Invalid handle.");
    return err - 2;
  }

  if (percent > 100.0 || percent < 0.0) {
    write_lock.unlock();
    registry_lock.unlock();
    LogErr(kFilename, sink_handle,
           "Failed to update statusbar: Invalid percentage.");
    return -7;
  }

  Statusbar& statusbar = _statusbar_registry[statusbar_handle.idx];

  if (idx >= statusbar.percentages.size()) {
    write_lock.unlock();
    registry_lock.unlock();
    LogErr(kFilename, sink_handle,
           "Failed to update statusbar: Invalid bar index.");
    return -8;
  }

  statusbar.percentages[idx] = percent;
  statusbar.spin_idxs[idx] = statusbar.spin_idxs[idx] + 1;
  int bar_error_code = _DrawStatusbarComponent(
      sink_handle, write_lock, percent, statusbar.bar_sizes[idx],
      statusbar.prefixes[idx], statusbar.postfixes[idx],
      statusbar.spin_idxs[idx], static_cast<int>(statusbar.positions[idx]));

  if (bar_error_code != kStatusbarLogSuccess && !statusbar.error_reported) {
    statusbar.error_reported = true;
    const char* why;
    switch (bar_error_code) {
      case -1:
        why = "Terminal width detection failed (Windows)";
        break;
      case -2:
        why = "Terminal width detection failed (Linux)";
        break;
      case -3:
        why = "Truncating was needed";
        break;
      case -4:
        why =
            "Terminal width detection failed (Windows) and truncation was "
            "needed";
        break;
      case -5:
        why =
            "Terminal width detection failed (Linux) and truncation was "
            "needed";
        break;
      default:
        why = "Unknown Error";
        break;
    }
    write_lock.unlock();
    registry_lock.unlock();
    LogErr(kFilename, sink_handle, "%s on statusbar with ID %u at bar idx %zu!",
           why, statusbar.id, idx);
  }

  write_lock.unlock();
  registry_lock.unlock();
  return kStatusbarLogSuccess;
}

};  // namespace statusbar_log
