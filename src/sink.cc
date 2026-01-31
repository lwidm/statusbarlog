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

// -- statusbarlog/src/sink.cc

// clang-format off

#include "statusbarlog/sink.h"

#include <cstdio>
#include <cstring>
#include <ios>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

#include "statusbarlog/os_posix.h"
#include "statusbarlog/statusbarlog.h"

// clang-format on

const std::string kFilename = "sink.cc";

namespace statusbar_log {
namespace sink {

namespace {

/**
 * \struct Sink
 *
 * \brief Sinkt struct contains both outputstreams and filestream as well as
 * identifier to validate associated handles and a mutex to make sure the sink
 * doesn't suffer from race conditions.
 *
 * \see SinkType: All possilbe sink types
 * \see SinkHandle: The handle to the sink
 */
typedef struct {
  std::mutex mutex;
  std::ostream* out;  ///< actual output stream used for printing
  std::unique_ptr<std::ofstream>
      owned_file;    ///< Some sinks need to own a file (nullptr otherwise)
  SinkType type;     ///< The sinks type
  std::string path;  ///< path for file-backed sinks (empty otherwise)
  int fd;  ///< File descriptor, used for differenciating between cout and cerr
           ///< (-1 if not applicable)
  unsigned int id;  ///< id of the struct, used for validating handles
} Sink;

std::vector<std::unique_ptr<Sink>> _sink_registry = {};
std::vector<SinkHandle> _sink_free_handles = {};
unsigned int _sink_handle_id_count = 0;

static std::mutex _sink_registry_mutex;
static std::mutex _sink_id_count_mutex;

/**
 * \brief Checks if a sink handle can be used for creating a new sink.
 *
 * This functions checks if the input handle is valid. If so the handle can not
 * be used for sink creation (as it already corresponds to another sink) and
 * returns a negative value. If the maximum number of sinks is reached this
 * function also returns a negative number.
 *
 * \param[in,out] sink_handle The handle to be checked for sink creation
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -1: Failed to create sink handle (handle already valid)
 *         - -2: Failed to create sink handle (handle registry exceeds
 */
int _ValidateSinkCreation(SinkHandle& sink_handle) {
  const int err = IsValidSinkHandle(sink_handle);
  if (err == kStatusbarLogSuccess) {
    std::cout << "ERROR [" << kFilename << "]: "
              << "Handle already is valid, cannot use it to create a new sink\n";
    return -1;
  }
  sink_handle.valid = false;
  sink_handle.id = 0;

  if (_sink_registry.size() - _sink_free_handles.size() >= kMaxSinkHandles) {
    std::cout
        << "ERROR [" << kFilename << "]: "
        << "Failed to create sink handle. Maximum number of sink handles ("
        << kMaxSinkHandles << ") reached";
    return -2;
  }

  return kStatusbarLogSuccess;
}

}  // namespace

int IsValidSinkHandle(const SinkHandle& sink_handle) {
  const std::size_t idx = sink_handle.idx;

  if (!sink_handle.valid) {
    return -1;
  }

  if ((sink_handle.idx >= _sink_registry.size()) ||
      (sink_handle.idx == SIZE_MAX)) {
    return -2;
  }
  if (sink_handle.id != _sink_registry[idx]->id) {
    return -3;
  }
  if (sink_handle.id == 0) {
    return -4;
  }
  return kStatusbarLogSuccess;
}

int IsValidSinkHandleVerbose(const SinkHandle& sink_handle) {
  const int is_valid_handle = IsValidSinkHandle(sink_handle);
  if (is_valid_handle == -1) {
    std::cout << "\033[999B\n";
    std::cout << "WARNING [" << kFilename
              << "]: Invalid sink handle: Valid flag set to false (idx: "
              << sink_handle.idx << ", ID: " << sink_handle.id << ")\n";
    return -1;
  }

  else if (is_valid_handle == -2) {
    std::cout << "\033[999B\n";
    std::cout << "WARNING [" << kFilename
              << "]: Invalid sink handle: Handle index " << sink_handle.idx
              << " out of bounds (max " << _sink_registry.size() << ")\n";
    return -2;
  }

  else if (is_valid_handle == -3) {
    std::cout << "\033[999B\n";
    std::cout << "WARNING [" << kFilename
              << "]: " << "Invalid sink Handle: ID mismatch: handle "
              << sink_handle.id << " vs registry "
              << _sink_registry[sink_handle.idx]->id;
    return -3;
  }

  else if (is_valid_handle == -4) {
    std::cout << "\033[999B\n";
    std::cout << "WARNING [" << kFilename
              << "]: " << "Invalid sink Handle: ID is 0 (i.e. invalid)";
    return -4;
  }

  else if (is_valid_handle != kStatusbarLogSuccess) {
    std::cout << "\033[999B\n";
    std::cout << "WARNING [" << kFilename
              << "]: " << "Invalid sink Handle: Errorcode not handled!";
    return -5;
  }

  return kStatusbarLogSuccess;
}

/**
 * \brief Flush the sink
 *
 * Function tries to flush a sink. Returns kStatusbarLogSuccess on success,
 * otherwise a negative integer
 *
 *\returns Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warnings codes:
 *         - statusbar_log::kStatusbarLogSuccess (i.e. 0): Successfully flushed
 * sink.
 *         - -1: Failed: Sink ostream not functional.
 *         - -2: Failed: Sink ostream became not functional after flushing.
 */
int _FlushSink(std::unique_ptr<Sink>& sink) {
  // std::lock_guard<std::mutex> lk(sink->mutex);
  if (sink->fd >= 0) {
    return kStatusbarLogSuccess;
  }
  if (!sink->out->good()) return -1;
  sink->out->flush();
  return sink->out->good() ? 0 : -2;
}

int CreateSinkStdout(SinkHandle& sink_handle) {
  const int err = _ValidateSinkCreation(sink_handle);
  if (err != kStatusbarLogSuccess) {
    return err;
  }

  std::unique_lock<std::mutex> registry_lock(_sink_registry_mutex,
                                             std::defer_lock);
  std::unique_lock<std::mutex> id_count_lock(_sink_id_count_mutex,
                                             std::defer_lock);
  std::lock(registry_lock, id_count_lock);

  _sink_handle_id_count++;
  if (_sink_handle_id_count == 0) {
    std::cout << "WARNING [" << kFilename
              << "]: Max number of possible sink handle ids reached, looping "
                 "back to 1\n";
    _sink_handle_id_count++;
  }

  if (!_sink_free_handles.empty()) {
    SinkHandle free_handle = _sink_free_handles.back();
    _sink_free_handles.pop_back();
    sink_handle.idx = free_handle.idx;
    // std::mutex
    _sink_registry[sink_handle.idx]->out = &std::cout;
    _sink_registry[sink_handle.idx]->owned_file.reset();
    _sink_registry[sink_handle.idx]->path.clear();
    _sink_registry[sink_handle.idx]->type = kSinkStdout;
    _sink_registry[sink_handle.idx]->fd = os_fileno_stdout();
    _sink_registry[sink_handle.idx]->id = _sink_handle_id_count;
  } else {
    std::unique_ptr<Sink> new_sink = std::make_unique<Sink>();
    sink_handle.idx = _sink_registry.size();
    new_sink->out = &std::cout;
    new_sink->owned_file.reset();
    new_sink->path.clear();
    new_sink->type = kSinkStdout;
    new_sink->fd = os_fileno_stdout();
    new_sink->id = _sink_handle_id_count;
    _sink_registry.push_back(std::move(new_sink));
  }
  sink_handle.id = _sink_handle_id_count;
  sink_handle.valid = true;

  return kStatusbarLogSuccess;
}

int CreateSinkFile(SinkHandle& sink_handle, const std::string path) {
  const int err = _ValidateSinkCreation(sink_handle);
  if (err != kStatusbarLogSuccess) {
    return err;
  }

  std::unique_lock<std::mutex> registry_lock(_sink_registry_mutex,
                                             std::defer_lock);
  std::unique_lock<std::mutex> id_count_lock(_sink_id_count_mutex,
                                             std::defer_lock);
  std::lock(registry_lock, id_count_lock);

  _sink_handle_id_count++;
  if (_sink_handle_id_count == 0) {
    std::cout << "WARNING [" << kFilename
              << "]: Max number of possible sink handle ids reached, looping "
                 "back to 1\n";
  }

  std::unique_ptr<std::ofstream> f;
  try {
    f = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
    if (!f->is_open() || !f->good()) {
      return -3;
    }
  } catch (...) {
    return -4;
  }

  if (!_sink_free_handles.empty()) {
    SinkHandle free_handle = _sink_free_handles.back();
    _sink_free_handles.pop_back();
    sink_handle.idx = free_handle.idx;
    // std::mutex
    _sink_registry[sink_handle.idx]->out = f.get();
    _sink_registry[sink_handle.idx]->owned_file = std::move(f);
    _sink_registry[sink_handle.idx]->path = path;
    _sink_registry[sink_handle.idx]->type = kSinkFileOwned;
    _sink_registry[sink_handle.idx]->fd = -1;
    _sink_registry[sink_handle.idx]->id = _sink_handle_id_count;
  } else {
    std::unique_ptr<Sink> new_sink = std::make_unique<Sink>();
    // std::mutex
    new_sink->out = f.get();
    new_sink->owned_file = std::move(f);
    new_sink->path = path;
    new_sink->type = kSinkFileOwned;
    new_sink->fd = -1;
    new_sink->id = _sink_handle_id_count;
    _sink_registry.push_back(std::move(new_sink));
  }
  sink_handle.id = _sink_handle_id_count;
  sink_handle.valid = true;

  return kStatusbarLogSuccess;
}

int CreateSinkOstream(SinkHandle& sink_handle, std::ostream& os) {
  const int err = _ValidateSinkCreation(sink_handle);
  if (err != kStatusbarLogSuccess) {
    return err;
  }

  std::unique_lock<std::mutex> registry_lock(_sink_registry_mutex,
                                             std::defer_lock);
  std::unique_lock<std::mutex> id_count_lock(_sink_id_count_mutex,
                                             std::defer_lock);
  std::lock(registry_lock, id_count_lock);

  _sink_handle_id_count++;
  if (_sink_handle_id_count == 0) {
    std::cout << "WARNING [" << kFilename
              << "]: Max number of possible sink handle ids reached, looping "
                 "back to 1\n";
  }

  int fd;
  if (&os == &std::cout) {
    fd = os_fileno_stdout();
  } else if (&os == &std::cerr) {
    fd = os_fileno_stderr();
  } else {
    fd = -1;
  }

  if (!_sink_free_handles.empty()) {
    SinkHandle free_handle = _sink_free_handles.back();
    _sink_free_handles.pop_back();
    sink_handle.idx = free_handle.idx;
    // std::mutex
    _sink_registry[sink_handle.idx]->out = &os;
    _sink_registry[sink_handle.idx]->owned_file.reset();
    _sink_registry[sink_handle.idx]->path.clear();
    _sink_registry[sink_handle.idx]->type = kSinkOstreamWrapped;
    _sink_registry[sink_handle.idx]->fd = fd;
    _sink_registry[sink_handle.idx]->id = _sink_handle_id_count;
  } else {
    std::unique_ptr<Sink> new_sink = std::make_unique<Sink>();
    // std::mutex
    new_sink->out = &os;
    new_sink->owned_file.reset();
    new_sink->path.clear();
    new_sink->type = kSinkOstreamWrapped;
    new_sink->fd = fd;
    new_sink->id = _sink_handle_id_count;
    _sink_registry.push_back(std::move(new_sink));
  }
  sink_handle.id = _sink_handle_id_count;
  sink_handle.valid = true;

  return kStatusbarLogSuccess;
}

SSIZE_T SinkWrite(const SinkHandle& sink_handle, const char* buf,
                  std::size_t len) {
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  if (!buf) return -1;

  if (!(IsValidSinkHandle(sink_handle) == kStatusbarLogSuccess)) return -2;

  std::unique_ptr<Sink>& sink = _sink_registry[sink_handle.idx];

  if (len == 0) return kStatusbarLogSuccess;

  if (!sink->out->good() && sink->fd < 0) return -3;

  if (sink->fd >= 0) {
#if defined(SSIZE_MAX)
    if (len > static_cast<std::size_t>(SSIZE_MAX)) return -2;
#endif
    SSIZE_T rc = os_write(sink->fd, buf, static_cast<size_t>(len));
    return rc;
  }

  if (!sink->out->good()) return -4;

  if (len >
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    return -5;
  }

  std::streambuf* sb = sink->out->rdbuf();
  if (!sb) return -6;

  std::streamsize want = static_cast<std::streamsize>(len);
  std::streamsize written = sb->sputn(buf, want);

  if (written != want) {
    sink->out->setstate(std::ios::failbit);
    return -7;
  }

  return static_cast<SSIZE_T>(written);
}

SSIZE_T SinkWriteStr(const SinkHandle& sink_handle, const std::string& str) {
  return SinkWrite(sink_handle, str.c_str(), str.size());
}

int DestroySinkHandle(SinkHandle& sink_handle) {
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) {
    std::cout << "ERROR [" << kFilename
              << "]: Failed to destory statusbar_handle!\n";
    return err;
  }

  // std::unique_lock<std::mutex> sink_lock;
  // get_unique_lock(sink_handle, sink_lock);
  // std::lock(sink_lock, registry_lock);
  std::lock_guard<std::mutex> registry_lock(_sink_registry_mutex);

  std::unique_ptr<Sink> target = std::move(_sink_registry[sink_handle.idx]);

  _FlushSink(target);

  target->out = nullptr;
  if (target->owned_file) {
    target->owned_file->close();
    if (!target->owned_file->good()) {
      // TODO: Error message (not sure if it will work here)
      return -6;
    }
    target->owned_file.reset();
  }
  target->type = kSinkInvalid;
  target->fd = -1;
  target->id = 0;

  _sink_registry[sink_handle.idx] = std::move(target);

  sink_handle.valid = false;
  sink_handle.id = 0;
  _sink_free_handles.push_back(sink_handle);

  return kStatusbarLogSuccess;
}

bool SinkIsTty(const SinkHandle& sink_handle) {
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) return false;

  auto& sink = _sink_registry[sink_handle.idx];  // reference, no move
  if (!sink) return false;

  if (sink->fd >= 0) {
    return os_isatty(sink->fd) != 0;
  }
  if (sink->out == &std::cout) {
    return os_isatty(os_fileno_stdout());
  }
  if (sink->out == &std::cerr) {
    return os_isatty(os_fileno_stderr());
  }

  return false;
}

int get_unique_lock(const SinkHandle& sink_handle,
                    std::unique_lock<std::mutex>& sink_lock) {
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  sink_lock = std::unique_lock<std::mutex>(
      _sink_registry[sink_handle.idx]->mutex, std::defer_lock);
  return kStatusbarLogSuccess;
}

int get_mutex_ptr(const SinkHandle& sink_handle, std::mutex*& sink_mutex_ptr) {
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  sink_mutex_ptr = &_sink_registry[sink_handle.idx]->mutex;
  return kStatusbarLogSuccess;
}

int get_sink_type(const SinkHandle& sink_handle, SinkType& sink_type) {
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  sink_type = _sink_registry[sink_handle.idx]->type;
  return kStatusbarLogSuccess;
}

int get_sink_type_silent(const SinkHandle& sink_handle, SinkType& sink_type) {
  int err = IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  std::lock_guard<std::mutex> lx(_sink_registry_mutex);
  sink_type = _sink_registry[sink_handle.idx]->type;
  return kStatusbarLogSuccess;
}

int SinkTellP(const SinkHandle& sink_handle, std::streampos* streampos){
  int err = IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  SinkType sink_type = _sink_registry[sink_handle.idx]->type;
  if (sink_type != kSinkFileOwned) return -5;
  *streampos = _sink_registry[sink_handle.idx]->owned_file->tellp();
  return kStatusbarLogSuccess;
}

int SinkSeekP(const SinkHandle& sink_handle, const std::streampos& streampos){
  int err = IsValidSinkHandle(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  SinkType sink_type = _sink_registry[sink_handle.idx]->type;
  if (sink_type != kSinkFileOwned) return -5;
  _sink_registry[sink_handle.idx]->owned_file->seekp(streampos);
  return kStatusbarLogSuccess;
}

int FlushSinkHandle(const SinkHandle& sink_handle) {
  int err = IsValidSinkHandleVerbose(sink_handle);
  if (err != kStatusbarLogSuccess) return err;
  err = _FlushSink(_sink_registry[sink_handle.idx]);
  if (err != kStatusbarLogSuccess) {
    return err + 5;
  }
  return kStatusbarLogSuccess;
}

int MoveCursorUp(const SinkHandle& sink_handle, int move) {
  if (move == 0) return kStatusbarLogSuccess;

  {
    std::lock_guard<std::mutex> registry_lock(_sink_registry_mutex);
    int valid = IsValidSinkHandle(sink_handle);
    if (valid != kStatusbarLogSuccess) return valid;
  }

  Sink* s = _sink_registry[sink_handle.idx].get();
  if (!s) return -6;

  // Case 1: we have an fd (covers stdout/stderr and any fd-backed sinks).
  if (s->fd >= 0) {
    std::string seq;
    if (move > 0) {
      seq = "\033[" + std::to_string(move) + "A";  // move up
    } else {
      seq.assign(static_cast<size_t>(-move), '\n');  // move down -> newlines
    }
    SSIZE_T rc = os_write(s->fd, seq.data(), seq.size());
    return (rc < 0) ? -7 : kStatusbarLogSuccess;
  }

  // Case 2: sink owns a file (CreateSinkFile -> kSinkFileOwned).
  // MoveCursorUp is not supported for file sinks.
  if (s->type == kSinkFileOwned) {
    std::cout << "ERROR [" << kFilename
              << "]: MoveCursorUp called on a file sink (not supported)\n";
    return -8;
  }

  // Case 3: wrapped ostream (kSinkOstreamWrapped or other non-fd)
  if (s->type == kSinkOstreamWrapped || s->out != nullptr) {
    std::string seq;
    if (move > 0) {
      seq = "\033[" + std::to_string(move) + "A";  // move up
    } else {
      seq.assign(static_cast<size_t>(-move), '\n');  // move down -> newlines
    }
    SSIZE_T rc = os_write(s->fd, seq.data(), seq.size());
    return (rc < 0) ? -7 : kStatusbarLogSuccess;
  } else {
    std::string buf(static_cast<unsigned int>(-move), '\n');
    SinkWriteStr(sink_handle, buf);
  }
  return kStatusbarLogSuccess;
}

}  // namespace sink
}  // namespace statusbar_log
