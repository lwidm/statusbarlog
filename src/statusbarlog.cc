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
#include <iosfwd>

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
 * - Relative positional rank within this group (larger = higher).
 * - Total width (characters) of each bar.
 * - Text displayed before each bar.
 * - Text displayed after each bar.
 * - Spinner animation indices.
 * - unique id corresponding to the handle
 * - Indicator whether error already has been reported
 */
// clang-format off
typedef struct {
  sink::SinkHandle sink_handle;         ///< The sink in which to print the statusbar (for e.g. stdout).
  std::vector<std::streampos> line_start_positions; ///< File write positions for each bar line (file sinks only).
  std::vector<double> percentages;      ///< Progress percentages (0-100) for each bar.
  std::vector<unsigned int> positions;  ///< Relative rank within this group (larger = higher).
  std::vector<unsigned int> bar_sizes;  ///< Total width (characters) of each bar.
  std::vector<std::string> prefixes;    ///< Text displayed before each bar.
  std::vector<std::string> postfixes;   ///< Text displayed after each bar.
  std::vector<std::size_t> spin_idxs;   ///< Spinner animation indices.
  std::size_t id;                       ///< unique id corresponding to the handle
  bool error_reported;                  ///< Indicator whether error already has been reported
} Statusbar;
// clang-format on

/**
 *
 */
std::vector<Statusbar> _statusbar_registry = {};
std::vector<StatusbarHandle> _statusbar_free_handles = {};
std::vector<std::size_t> _statusbar_stack_order = {};
std::size_t _statusbar_handle_id_count = 0;

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
           "Invalid handle: Valid flag set to false (idx: %zu, ID: %zu)",
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
           "Invalid Handle: ID mismatch: handle %u vs registry %zu",
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
 * \brief Builds the formatted string for a single statusbar component.
 *
 * Generates a string of the form: prefix[####/    ]  50.00postfix
 * Truncates to terminal width if the sink is a TTY.
 *
 * \param[out] status_str Receives the formatted bar string.
 * \param[in] sink_handle Sink handle (used to check if TTY for truncation).
 * \param[in] write_lock Unique lock of the sink (must be locked).
 * \param[in] percent Progress percentage (0-100).
 * \param[in] bar_width Width of the bar in characters.
 * \param[in] prefix Text before the bar.
 * \param[in] postfix Text after the bar.
 * \param[in, out] spinner_idx Spinner animation index (wrapped to 0-3).
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -1: Terminal width detection failed (Windows)
 *         - -2: Terminal width detection failed (Linux)
 *         - -3: Truncation was needed (bar exceeds terminal width)
 *         - -4: Terminal width detection failed (Windows) AND truncation needed
 *         - -5: Terminal width detection failed (Linux) AND truncation needed
 *         - -7: write_lock not owned
 */
int _StatusbarComponentString(
    std::string& status_str, const sink::SinkHandle& sink_handle,
    std::unique_lock<std::mutex>& write_lock, const double percent,
    const unsigned int bar_width, const std::string& prefix,
    const std::string& postfix, std::size_t& spinner_idx) {
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
  status_str = oss.str();

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

  return err;
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

  int err = kStatusbarLogSuccess;

  std::string status_str;
  err = _StatusbarComponentString(status_str, sink_handle, write_lock, percent,
                                  bar_width, prefix, postfix, spinner_idx);

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
 * \brief Computes the absolute terminal layout of all statusbar groups for a
 * given sink type.
 *
 * Walks _statusbar_stack_order (creation order; front = oldest, back = newest),
 * keeps only statusbars whose sink type matches \p sink_type, and assigns each
 * bar an absolute offset in lines above the cursor (1 = the line directly above
 * the cursor). Groups are stacked newest-nearest-the-cursor; within a group,
 * larger \c positions values are placed higher.
 *
 * \warning The caller must hold _statusbar_registry_mutex. This function only
 * reads the registry; it performs no locking and no drawing.
 *
 * \param[in]  sink_type   Only statusbars on a sink of this type are included.
 * \param[out] handle_idx  Registry index of each bar's statusbar.
 * \param[out] bar_idx     Bar component index within that statusbar.
 * \param[out] offset      Lines above the cursor (1 = the line directly above
 *                         , smalles offset first).
 * \param[out] total_lines Total number of lines the stack occupies.
 *
 * \return statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or one of
 * these error codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success
 *         - -1: A stack-order entry was out of registry bounds
 *         - -2: A stack-order entry referenced a destroyed statusbar (id 0)
 *         - -3: Failed to query a statusbar's sink type
 *
 * \note On a negative return the output vectors hold only partially computed
 * data and must not be used.
 */
int _ComputeStackLayout(const sink::SinkType& sink_type,
                        std::vector<std::size_t>& handle_idx,
                        std::vector<std::size_t>& bar_idx,
                        std::vector<int>& offset, unsigned int& total_lines) {
  int err = kStatusbarLogSuccess;
  if (handle_idx.size() > 0) handle_idx.clear();
  if (bar_idx.size() > 0) bar_idx.clear();
  if (offset.size() > 0) offset.clear();
  total_lines = 0;
  if (_statusbar_stack_order.size() == 0) return kStatusbarLogSuccess;

  for (std::size_t s = _statusbar_stack_order.size(); s-- > 0;) {
    const std::size_t i = _statusbar_stack_order[s];
    if (i >= _statusbar_registry.size()) return -1;
    if (_statusbar_registry[i].id == 0) return -2;

    sink::SinkType compare_sink_type;
    err = sink::get_sink_type_silent(_statusbar_registry[i].sink_handle,
                                     compare_sink_type);
    if (err != kStatusbarLogSuccess) return -3;
    if (compare_sink_type != sink_type) continue;

    const std::vector<unsigned int>& positions =
        _statusbar_registry[i].positions;
    const std::size_t n_bars = positions.size();

    std::vector<std::size_t> bar_order(n_bars);
    for (std::size_t j = 0; j < n_bars; j++) bar_order[j] = j;
    std::sort(bar_order.begin(), bar_order.end(),
              [&positions](std::size_t a, std::size_t b) {
                return positions[a] < positions[b];
              });
    for (std::size_t rank = 0; rank < n_bars; rank++) {
      handle_idx.push_back(i);
      bar_idx.push_back(bar_order[rank]);
      offset.push_back(static_cast<int>(total_lines) + static_cast<int>(rank) +
                       1);
    }
    total_lines += static_cast<unsigned int>(n_bars);
  }

  return err;
}

/**
 * \brief Redraws all statusbar components in place for a file sink.
 *
 * Seeks to the top bar's recorded line position and rewrites each bar line
 * sequentially top-to-bottom, updating line_start_positions via tellp as it
 * goes.
 *
 * \param[in] sink_handle File sink handle to write to.
 * \param[in] write_lock Unique lock of the sink (must be locked).
 * \param[in] lay_handle_idx Registry indices of the bars to draw, ordered
 * bottom-to-top as produced by _ComputeStackLayout (iterated in reverse so the
 * file lines run top-to-bottom).
 * \param[in] lay_bar_idx Matching bar-component indices for lay_handle_idx.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -7: write_lock not owned
 *         - -8: Sink write failed
 */
int _DrawStatusbarsOwnedFile(const sink::SinkHandle& sink_handle,
                             std::unique_lock<std::mutex>& write_lock,
                             const std::vector<std::size_t>& lay_handle_idx,
                             const std::vector<std::size_t>& lay_bar_idx) {
  if (!write_lock.owns_lock()) {
    return -7;
  }

  int err = kStatusbarLogSuccess;

  if (lay_handle_idx.empty()) return err;

  std::streampos& p_line_pos = _statusbar_registry[lay_handle_idx.back()]
                                   .line_start_positions[lay_bar_idx.back()];
  sink::SinkSeekP(sink_handle, p_line_pos);

  std::string status_str;
  for (std::size_t m = lay_handle_idx.size(); m-- > 0;) {
    const std::size_t i = lay_handle_idx[m];
    const std::size_t j = lay_bar_idx[m];
    sink::SinkTellP(sink_handle,
                    &_statusbar_registry[i].line_start_positions[j]);

    err = _StatusbarComponentString(status_str, sink_handle, write_lock,
                                    _statusbar_registry[i].percentages[j],
                                    _statusbar_registry[i].bar_sizes[j],
                                    _statusbar_registry[i].prefixes[j],
                                    _statusbar_registry[i].postfixes[j],
                                    _statusbar_registry[i].spin_idxs[j]);
    std::ostringstream oss;
    oss << status_str << '\n';
    SSIZE_T written = sink::SinkWriteStr(sink_handle, oss.str());
    if (written <= 0) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Sink Write Failed in _DrawStatusbarsOwnedFile!\n";
      return -8;
    }
  }

  _ConditionalFlush(sink_handle);

  return err;
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

// TODO: files can only have statusbars that are directly next to each other
// (The position mearly refers to ordering, not location)
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
    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err != kStatusbarLogSuccess) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Failed to compute stack layout in LogV (owned file sink) "
                   "(_ComputeStackLayout error code \""
                << err << "\")!\n";
      return -8;
    }

    if (!lay_handle_idx.empty()) {
      sink::SinkSeekP(sink_handle,
                      _statusbar_registry[lay_handle_idx.back()]
                          .line_start_positions[lay_bar_idx.back()]);
    }

    SSIZE_T written = sink::SinkWriteStr(sink_handle, formatted_message);
    if (written <= 0) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Sink Write Failed in LogV (owned file sink)!\n";
      return -6;
    }
    _ConditionalFlush(sink_handle);

    if (!lay_handle_idx.empty()) {
      sink::SinkTellP(sink_handle,
                      &_statusbar_registry[lay_handle_idx.back()]
                           .line_start_positions[lay_bar_idx.back()]);
      _DrawStatusbarsOwnedFile(sink_handle, write_lock, lay_handle_idx,
                               lay_bar_idx);
    }
  } else {
    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err < 0) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Failed to compute stack layout in LogV "
                   "(_ComputeStackLayout error code \""
                << err << "\")!\n";
      return -8;
    }

    int move = static_cast<int>(total_lines);

    sink::MoveCursorUp(sink_handle, move);
    if (total_lines > 0) printf("\r\033[2K\r");

    SSIZE_T written = sink::SinkWriteStr(sink_handle, formatted_message);
    if (written <= 0) {
      std::cout << "ERROR [" << kFilename << "]: "
                << "Sink Write Failed in LogV!\n";
      return -7;
    }
    _ConditionalFlush(sink_handle);

    sink::MoveCursorUp(sink_handle, -move);
    for (std::size_t k = 0; k < lay_handle_idx.size(); ++k) {
      const std::size_t i = lay_handle_idx[k];
      const std::size_t j = lay_bar_idx[k];
      int bar_err_code = _DrawStatusbarComponent(
          sink_handle, write_lock, _statusbar_registry[i].percentages[j],
          _statusbar_registry[i].bar_sizes[j],
          _statusbar_registry[i].prefixes[j],
          _statusbar_registry[i].postfixes[j],
          _statusbar_registry[i].spin_idxs[j], lay_offset[k]);
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
          return bar_err_code - 8;
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
           "Failed to create statusbar_handle! Failed to get sink mutex ptr. "
           "Errorcode: %d",
           err);
    return -5;
  }

  for (std::size_t i = 0; i < _positions.size(); ++i) {
    for (std::size_t j = i + 1; j < _positions.size(); ++j) {
      if (_positions[i] == _positions[j]) {
        LogErr(kFilename, sink_handle,
               "Failed to create statusbar_handle: position values within a "
               "group must be distinct.");
        return -6;
      }
    }
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

  sink::SinkType sink_type;
  sink::get_sink_type_silent(sink_handle, sink_type);

  std::vector<std::streampos> line_start_positions;
  if (sink_type == sink::kSinkFileOwned) {
    line_start_positions.resize(num_bars);
    for (auto& start_position : line_start_positions) {
      sink::SinkTellP(sink_handle, &start_position);
    }
  }
  if (!_statusbar_free_handles.empty()) {
    StatusbarHandle free_handle = _statusbar_free_handles.back();
    _statusbar_free_handles.pop_back();
    statusbar_handle.idx = free_handle.idx;
    _statusbar_registry[statusbar_handle.idx] = {sink_handle,
                                                 line_start_positions,
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
        Statusbar{sink_handle, line_start_positions, percentages, _positions,
                  sanitized_bar_sizes, sanitized_prefixes, sanitized_postfixes,
                  spin_idxs, _statusbar_handle_id_count, false});
  }
  _statusbar_stack_order.push_back(statusbar_handle.idx);

  statusbar_handle.id = _statusbar_handle_id_count;
  statusbar_handle.valid = true;

  if (sink_type == sink::kSinkFileOwned) {
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

    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err == kStatusbarLogSuccess && !lay_handle_idx.empty()) {
      sink::SinkTellP(sink_handle,
                      &_statusbar_registry[lay_handle_idx.back()]
                           .line_start_positions[lay_bar_idx.back()]);

      _DrawStatusbarsOwnedFile(sink_handle, write_lock, lay_handle_idx,
                               lay_bar_idx);
    }

  } else {
    sink::MoveCursorUp(sink_handle, -static_cast<int>(num_bars));

    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err == kStatusbarLogSuccess) {
      for (std::size_t k = 0; k < lay_handle_idx.size(); ++k) {
        const std::size_t i = lay_handle_idx[k];
        const std::size_t j = lay_bar_idx[k];
        _DrawStatusbarComponent(
            sink_handle, write_lock, _statusbar_registry[i].percentages[j],
            _statusbar_registry[i].bar_sizes[j],
            _statusbar_registry[i].prefixes[j],
            _statusbar_registry[i].postfixes[j],
            _statusbar_registry[i].spin_idxs[j], lay_offset[k]);
      }
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
  std::erase(_statusbar_stack_order, statusbar_handle.idx);
  if (sink_type != sink::kSinkFileOwned) {
    const int k = static_cast<int>(target.positions.size());

    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    int lerr = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                                   lay_offset, total_lines);
    if (lerr == kStatusbarLogSuccess) {
      for (std::size_t m = 0; m < lay_handle_idx.size(); ++m) {
        const std::size_t i = lay_handle_idx[m];
        const std::size_t j = lay_bar_idx[m];
        _DrawStatusbarComponent(
            sink_handle, write_lock, _statusbar_registry[i].percentages[j],
            _statusbar_registry[i].bar_sizes[j],
            _statusbar_registry[i].prefixes[j],
            _statusbar_registry[i].postfixes[j],
            _statusbar_registry[i].spin_idxs[j], lay_offset[m]);
      }
      // clear the lines the destroyed group's bars used to occupy
      for (int o = static_cast<int>(total_lines) + 1;
           o <= static_cast<int>(total_lines) + k; ++o) {
        sink::MoveCursorUp(sink_handle, o);
        ClearCurrentLine(sink_handle);
        sink::MoveCursorUp(sink_handle, -o);
      }
    }
  }
  sink::FlushSinkHandle(sink_handle);

  target.sink_handle = sink::SinkHandle();
  target.line_start_positions.clear();
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

  if (sink_type == sink::kSinkFileOwned) {
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

    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err != kStatusbarLogSuccess) {
      write_lock.unlock();
      registry_lock.unlock();
      LogErr(kFilename, sink_handle,
             "Failed to update statusbar: layout computation failed (%d).", err);
      return -9;
    }

    if (!lay_handle_idx.empty()) {
      _DrawStatusbarsOwnedFile(sink_handle, write_lock, lay_handle_idx,
                               lay_bar_idx);
    }
  } else {
    std::vector<std::size_t> lay_handle_idx, lay_bar_idx;
    std::vector<int> lay_offset;
    unsigned int total_lines;
    err = _ComputeStackLayout(sink_type, lay_handle_idx, lay_bar_idx,
                              lay_offset, total_lines);
    if (err != kStatusbarLogSuccess) {
      write_lock.unlock();
      registry_lock.unlock();
      LogErr(kFilename, sink_handle,
             "Failed to update statusbar: layout computation failed (%d).",
             err);
      return -9;
    }
    int move = 0;
    for (std::size_t k = 0; k < lay_handle_idx.size(); ++k) {
      if (lay_handle_idx[k] == statusbar_handle.idx && lay_bar_idx[k] == idx) {
        move = lay_offset[k];
        break;
      }
    }

    int bar_error_code = _DrawStatusbarComponent(
        sink_handle, write_lock, percent, statusbar.bar_sizes[idx],
        statusbar.prefixes[idx], statusbar.postfixes[idx],
        statusbar.spin_idxs[idx], move);

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
      LogErr(kFilename, sink_handle,
             "%s on statusbar with ID %zu at bar idx %zu!", why, statusbar.id,
             idx);
    }
  }

  write_lock.unlock();
  registry_lock.unlock();
  return kStatusbarLogSuccess;
}
};  // namespace statusbar_log
