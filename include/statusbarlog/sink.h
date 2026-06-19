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

// -- statusbarlog/include/statusbarlog/sink.h

#ifndef STATUSBARLOG_SINK_H_
#define STATUSBARLOG_SINK_H_

#include <mutex>
#include <string>

#include "statusbarlog/os_posix.h"

namespace statusbar_log {
namespace sink {

constexpr unsigned int kMaxSinkHandles = 20;

/**
 * \enum SinkType
 * \brief All possible sink types.
 */
typedef enum {
  kSinkInvalid,        ///< Invalid sink type
  kSinkStdout,         ///< Sink linked to stdcout (non owning)
  kSinkFileOwned,      ///< Sink linked to a file (owning)
  kSinkOstreamWrapped  /// Sink wrapped around existing arbitrary ostream (non
                       /// owning)
} SinkType;

/**
 * \struct SinkHandle
 * \brief Handle to a Sink. Used to interact with the underlying sink
 * struct without directly touching it
 *
 * \see Sink: Underlying sink struct
 */
typedef struct {
  std::size_t idx;  ///< Positional index corresponding to the sinks
                    ///< position in the registry
  std::size_t id;   ///< ID of the sink. Must be unique. Used to verify
                    ///< validity of statusbar
  bool valid;       ///< weather or not this sink is valid (for e.g. false after
                    ///< destruction)
} SinkHandle;

/**
 * \brief Check if the argument is a valid sink handle
 *
 * This functions performs a test on a SinkHandle and returns
 * statusbar_log::kStatusbarLogSuccess (i.e. 0) if it is a valid handle and a
 * negative number otherwise
 *
 * \param[in] SinkHandle struct to be checked for validity
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
int IsValidSinkHandle(const SinkHandle& sink_handle);

/**
 * \brief Check if the argument is a valid sink handle and prints an error
 * message.
 *
 * This functions performs a test on a SinkHandle and returns
 * statusbar_log::kStatusbarLogSuccess (i.e. 0) if it is a valid handle and a
 * negative number otherwise. Same as _IsValidHandle but also prints an error
 * message.
 *
 * \param[in] SinkHandle struct to be checked for validity
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
int IsValidSinkHandleVerbose(const SinkHandle& sink_handle);

/**
 * \brief Initialises a sink that wraps std::cout (does not take ownership) and
 * updates its handle.
 *
 * This function takes an empty SinkHandle struct and creates an associated sink
 * that wraps std::cout (does not take ownership)
 *
 * \param[out] sink_handle Struct to initialize.
 *
 * \warning Don't forget to destroy the sink_handle after use.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -1: Failed to create sink handle (handle already valid)
 *         - -2: Failed to create sink handle (handle registry exceeds
 * maximum element limit)
 *
 * \see Sink: The sink struct.
 * \see _sink_registry: The registry for sink struct in use.
 * \see _sink_free_handles: The registry for free sink handles.
 */
int CreateSinkStdout(SinkHandle& sink_handle);

/**
 * \brief Initialises a sink that opens/owns the given file path.
 *
 * This function takes an empty SinkHandle struct and creates an associated sink
 * that opens/owns a file path (takes ownership). The file is opened with
 * std::ios::out | std::ios::trunc, which truncates any existing content and
 * allows seekable writes (required for in-place statusbar updates).
 *
 * \param[out] sink_handle Struct to initialize.
 * \param[in] path Path to the file to be opened/created.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success (no errors)
 *         - -1: Failed to create sink handle (handle already valid)
 *         - -2: Failed to create sink handle (handle registry exceeds
 * maximum element limit)
 *         - -3: Failed to create sink handle (failed in opening file)
 *         - -4: Failed to create sink handle (unknown error in opening file)
 *
 * \warning Don't forget to destroy the sink_handle after use.
 *
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct.
 * \see _sink_registry: The registry for sink struct in use.
 * \see _sink_free_handles: The registry for free sink handles.
 */
int CreateSinkFile(SinkHandle& sink_handle, const std::string path);

/**
 * \brief Destorys a Sink using its handle and invalidates it.
 *
 * This function takes a Sinkhandle, clears its content, adds it
 * to the _statusbar_free_handles registry and frees its position in the
 * _statusbar_registry.
 *
 *
 * \param[in, out] sink_handle Struct to destroy.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warning codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Successfull
 * destruction
 *         - -1: Couldn't destroy sink: Invalid handle - Valid flag of handle
 * set to false
 *         - -2: Couldn't destroy sink: Invalid handle - Handle index out of
 * bounds in `statusbar_registry`
 *         - -3: Couldn't destroy sink: Invalid handle - Handle IDs don't match
 * between handle struct
 *         - -4: Couldn't destroy sink: Invalid handle - Handle ID is 0 (i.e.
 * invalid)
 *         - -5: Couldn't destroy sink: Invalid handle - Errorcode not handled
 *         - -6: Failed to handle destruction of owned_file.
 *
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct.
 * \see _sink_registry: The registry for statusbar struct in use.
 * \see _sink_free_handles: The registry for free statusbar handles.
 * \see CreateSinkStdout: Creating new stdout sink.
 * \see CreateSinkFile: Creating new file sink.
 */
int DestroySinkHandle(SinkHandle& sink_handle);

/**
 * \brief Write len bytes (returns number of bytes written or -1 on error).
 */
SSIZE_T SinkWrite(const SinkHandle& sink_handle, const char* buf,
                  std::size_t len);

/**
 * Convenience to write a NUL-terminated string (returns 0 on success, -1 on
 * error).
 */
SSIZE_T SinkWriteStr(const SinkHandle& sink_handle, const std::string& str);

/**
 * \brief Flush a sink using its handle
 *
 * Function tries to flush a sink using its handle. Returns kStatusbarLogSuccess
 * on success, otherwise a negative integer
 *
 *\returns Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these error/warnings codes:
 *         - statusbar_log::kStatusbarLogSuccess (i.e. 0): Successfully flushed
 * sink.
 *         - -1: Couldn't flush sink: Invalid handle (Valid flag of handle
 * set to false)
 *         - -2: Couldn't flush sink: Invalid handle (Handle index out of
 * bounds in `statusbar_registry`)
 *         - -3: Couldn't flush sink: Invalid handle (Handle IDs don't match
 * between handle struct)
 *         - -4: Couldn't flush sink: Invalid handle (Handle ID is 0 (i.e.
 * invalid))
 *         - -5: Couldn't flush sink: Invalid handle (Errorcode not handled)
 *         - -6: Failed: Sink ostream not functional.
 *         - -7: Failed: Sink ostream became not functional after flushing.
 */
int FlushSinkHandle(const SinkHandle& sink_handle);

/**
 * Returns true if the underlying stream is a TTY (best-effort).
 */
bool SinkIsTty(const SinkHandle& sink_handle);

/**
 * \brief Get a unique lock of the mutex associated to the sink handle.
 *
 * This function is used to retrieve a (unlocked) unique_lock of the mutex of
 * the sink assocated to the handle.
 *
 * \param[in] sink_handle Sink handle struct of which to get the unique_lock.
 * \param[in, out]  sink_mutex_ptr Object in which the unique_lock will be
 saved to.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the lock
 * retrieval succeeded, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Failed: Invalid handle (Valid flag of handle set to false)
 *         - -2: Failed: Invalid handle (Handle index out of bounds in
 * `statusbar_registry)`
 *         - -3: Failed: Invalid handle (Handle IDs don't match between handle
 * struct)
 *         - -4: Failed: Invalid handle (Handle ID is 0 (i.e. invalid))
 *         - -5: Failed: Invalid handle (Errorcode not handled)
 *
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct
 */
int get_unique_lock(const SinkHandle& sink_handle,
                    std::unique_lock<std::mutex>& lock);

/**
 * \brief Get the pointer to the mutex associated to the sink handle.
 *
 * This function is used to retrieve a pointer to the mutex of
 * the sink assocated to the handle.
 *
 * \param[in] sink_handle Sink handle struct of which to get the mutex.
 * \param[in, out]  sink_mutex_ptr Pointer in which the mutex pointer will be
 saved to.

 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the lock
 * retrieval succeeded, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Failed: Invalid handle (Valid flag of handle set to false)
 *         - -2: Failed: Invalid handle (Handle index out of bounds in
 * `statusbar_registry)`
 *         - -3: Failed: Invalid handle (Handle IDs don't match between handle
 * struct)
 *         - -4: Failed: Invalid handle (Handle ID is 0 (i.e. invalid))
 *         - -5: Failed: Invalid handle (Errorcode not handled)
 *
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct
 */
int get_mutex_ptr(const SinkHandle& sink_handle, std::mutex*& sink_mutex_ptr);

/**
 * \brief Get the sink type associated to the sink handle.
 *
 * This function is used to retrieve a the sink type of the sink associated to
 * the handle
 *
 * \param[in] sink_handle Sink handle struct of which to get the type.
 * \param[in, out] sink_type SinkType struct in which to save type.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the sink
 * type retrieval succeeded, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Failed: Invalid handle (Valid flag of handle set to false)
 *         - -2: Failed: Invalid handle (Handle index out of bounds in
 * `statusbar_registry)`
 *         - -3: Failed: Invalid handle (Handle IDs don't match between handle
 * struct)
 *         - -4: Failed: Invalid handle (Handle ID is 0 (i.e. invalid))
 *         - -5: Failed: Invalid handle (Errorcode not handled)
 *
 * \see SinkType: All possible sink types
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct
 */
int get_sink_type(const SinkHandle& sink_handle, SinkType& sink_type);

/**
 * \brief Get the current put-pointer position of a file sink.
 *
 * Retrieves the current write position (tellp) of the owned file stream
 * associated with the sink. Only valid for file sinks (kSinkFileOwned).
 *
 * \param[in] sink_handle Sink handle to query.
 * \param[out] streampos Receives the current write position.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success
 *         - -1: Invalid handle (valid flag set to false)
 *         - -2: Invalid handle (index out of bounds)
 *         - -3: Invalid handle (IDs don't match)
 *         - -4: Invalid handle (ID is 0)
 *         - -5: Sink is not a file sink (kSinkFileOwned)
 *
 * \see SinkSeekP: Set the put-pointer position.
 */
int SinkTellP(const SinkHandle& sink_handle, std::streampos* streampos);

/**
 * \brief Set the put-pointer position of a file sink.
 *
 * Moves the write position (seekp) of the owned file stream associated with
 * the sink. Only valid for file sinks (kSinkFileOwned).
 *
 * \param[in] sink_handle Sink handle to modify.
 * \param[in] streampos The target write position to seek to.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success, or
 * one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Success
 *         - -1: Invalid handle (valid flag set to false)
 *         - -2: Invalid handle (index out of bounds)
 *         - -3: Invalid handle (IDs don't match)
 *         - -4: Invalid handle (ID is 0)
 *         - -5: Sink is not a file sink (kSinkFileOwned)
 *
 * \see SinkTellP: Get the current put-pointer position.
 */
int SinkSeekP(const SinkHandle& sink_handle, const std::streampos& streampos);

/**
 * \brief Get the sink type associated to the sink handle.
 *
 * This function is used to retrieve a the sink type of the sink associated to
 * the handle while suppressing warnings generated by the validity check.
 *
 * \param[in] sink_handle Sink handle struct of which to get the type.
 * \param[in, out] sink_type SinkType struct in which to save type.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) if the sink
 * type retrieval succeeded, or one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Failed: Invalid handle (Valid flag of handle set to false)
 *         - -2: Failed: Invalid handle (Handle index out of bounds in
 * `statusbar_registry)`
 *         - -3: Failed: Invalid handle (Handle IDs don't match between handle
 * struct)
 *         - -4: Failed: Invalid handle (Handle ID is 0 (i.e. invalid))
 *         - -5: Failed: Invalid handle (Errorcode not handled)
 *
 * \see SinkType: All possible sink types
 * \see SinkHandle: The sink handle struct
 * \see Sink: The sink struct
 */
int get_sink_type_silent(const SinkHandle& sink_handle, SinkType& sink_type);

/**
 * \brief Move the cursor for the given sink up (positive) or down (negative).
 *
 * For TTY sinks this emits the ANSI sequence "\033[<N>A" to move the cursor up
 * N lines. For moving down it writes N newline characters.
 *
 * Behavior:
 * - If the sink has a valid file descriptor (fd >= 0) writes directly to that
 *   fd.
 * - If the sink wraps std::cout or std::cerr, writes to fileno(stdout|stderr).
 * - If the sink is a file sink (kSinkFileOwned), returns an error (not
 *   supported).
 *
 * \param[in] sink_handle Sink handle struct of which to get the type.
 * \param[in] move number of lines to move up (positive value) or down (negative
 * value).
 *
 * This function avoids blocking and therefore reduces the risk of deadlocks
 * when called from code that may already hold a sink lock.
 *
 * \return Returns statusbar_log::kStatusbarLogSuccess (i.e. 0) on success.
 * Otherwise one of these status codes:
 *         -  statusbar_log::kStatusbarLogSuccess (i.e. 0): Valid handle
 *         - -1: Failed: Invalid handle (Valid flag of handle set to false)
 *         - -2: Failed: Invalid handle (Handle index out of bounds in
 * `statusbar_registry)`
 *         - -3: Failed: Invalid handle (Handle IDs don't match between handle
 * struct)
 *         - -4: Failed: Invalid handle (Handle ID is 0 (i.e. invalid))
 *         - -5: Failed: Invalid handle (Errorcode not handled)
 *         - -6: Failed: Could not obtain sink pointer from registry.
 *         - -7: Failed: Failed to write to fd-backed sink.
 *         - -8: Failed: MoveCursorUp not supported for file sinks.
 */
int MoveCursorUp(const SinkHandle& sink_handle, int move);

}  // namespace sink
}  // namespace statusbar_log

#endif  // !STATUSBARLOG_SINK_H_
