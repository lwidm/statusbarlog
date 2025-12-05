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

// -- statusbarlog/include/statusbarlog/os_posix.h

#ifndef STATUSBARLOG_OS_POSIX_H_
#define STATUSBARLOG_OS_POSIX_H_

#include <cstdio>

#include <fcntl.h>

#ifdef _WIN32
  #include <io.h>       // _open, _close, _dup, _dup2, _read, _fileno, _pipe, _isatty
  #include <sys/stat.h>
  #include <BaseTsd.h>
  #define STATUSBARLOG_OPEN _open
  #define STATUSBARLOG_CLOSE _close
  #define STATUSBARLOG_DUP _dup
  #define STATUSBARLOG_DUP2 _dup2
  #define STATUSBARLOG_READ _read
  #define STATUSBARLOG_WRITE _write
  #define STATUSBARLOG_PIPE _pipe
  #define STATUSBARLOG_FILENO _fileno
  #define STATUSBARLOG_ISATTY _isatty
  #ifndef S_IRUSR
    #define S_IRUSR _S_IREAD
  #endif
#else
  #include <unistd.h>
  #define STATUSBARLOG_OPEN open
  #define STATUSBARLOG_CLOSE close
  #define STATUSBARLOG_DUP dup
  #define STATUSBARLOG_DUP2 dup2
  #define STATUSBARLOG_READ read
  #define STATUSBARLOG_WRITE write
  #define STATUSBARLOG_PIPE pipe
  #define STATUSBARLOG_FILENO fileno
  #define STATUSBARLOG_ISATTY isatty
#endif

#include <string>
#include <cstring>

namespace statusbar_log {

#ifdef _WIN32
typedef long SSIZE_T;
#else
typedef ssize_t SSIZE_T;
#endif

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable : 4996) // deprecated "usafe" CRT
#endif
static inline int os_open(const char* path, int flags, int mode) {
  return STATUSBARLOG_OPEN(path, flags, mode);
}
#ifdef _WIN32
# pragma warning(pop)
#endif
static inline int os_close(int fd) { return STATUSBARLOG_CLOSE(fd); }
static inline int os_dup(int fd) { return STATUSBARLOG_DUP(fd); }
static inline int os_dup2(int fd, int fd2) { return STATUSBARLOG_DUP2(fd, fd2); }
static inline int os_pipe(int pipefd[2]) {
#ifdef _WIN32
  return STATUSBARLOG_PIPE(pipefd, 4096, 0);
#else
  return STATUSBARLOG_PIPE(pipefd);
#endif
}
static inline SSIZE_T os_read(int fd, void* buf, size_t count) {
  return STATUSBARLOG_READ(fd, buf, (unsigned int)count);
}
static inline int os_fileno_stdout() { return STATUSBARLOG_FILENO(stdout); }
static inline int os_fileno_stderr() { return STATUSBARLOG_FILENO(stderr); }
static inline int os_isatty(int fd) { return STATUSBARLOG_ISATTY(fd); }

static inline SSIZE_T os_write(int fd, const void* buf, size_t count) {
#ifdef _WIN32
    return STATUSBARLOG_WRITE(fd, (const char*)buf, (unsigned int)count);
#else
    return STATUSBARLOG_WRITE(fd, buf, count);
#endif
}

inline std::string GetErrStr() {
#ifdef _WIN32
    char buf[256];
    if (strerror_s(buf, sizeof(buf), errno) != 0) {
        strncpy_s(buf, sizeof(buf), "Unknown error", _TRUNCATE);
    }
    return std::string(buf);
#else
    return std::string(std::strerror(errno));
#endif
}

} // namespace: statusbar_log

#endif // STATUSBARLOG_OS_POSIX_H_

