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

// -- statusbarlog/test/src/statusbarlog_test.cc

// clang-format off

#include <gtest/gtest.h>

#include <vector>
#include <string>

#include "statusbarlog/statusbarlog.h"
#include "statusbarlog/sink.h"
#include "statusbarlog_test.h"

// clang-format on

const std::string kFilename = "statusbarlog_test.cc";

// ==================================================
// Base Test Fixture with Auto Log Filename Generation
// ==================================================

class StatusbarTestBase : public ::testing::Test {
 protected:
  statusbar_log::sink::SinkHandle sink_handle_;
  std::string GetTestLogFilename() {
    const auto* test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return statusbar_log::test::GenerateTestLogFilename(
        test_info->test_suite_name(), test_info->name());
  }
  void SetUp() override { statusbar_log::sink::CreateSinkStdout(sink_handle_); }
  void TearDown() override {
    statusbar_log::sink::DestroySinkHandle(sink_handle_);
  }
};

// ==================================================
// HandleManagementTest
// ==================================================

class HandleManagementTest : public StatusbarTestBase {};

TEST_F(HandleManagementTest, CreateSingleBarHandle) {
  statusbar_log::StatusbarHandle handle;
  std::vector<unsigned int> positions = {1};
  std::vector<unsigned int> bar_sizes = {50};
  std::vector<std::string> prefixes = {"Processing"};
  std::vector<std::string> postfixes = {"items"};

  const std::string log_filename = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename);
  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "CreateStatusbarHandle should return "
         "statusbar_log::kStatusbarLogSuccess";
  EXPECT_TRUE(handle.valid)
      << "Handle should be marked as valid after creation";
  EXPECT_NE(handle.id, 0) << "Handle should have a non-zero ID assigned";
  EXPECT_LT(handle.id, SIZE_MAX) << "Handle index should be a valid value";
  EXPECT_LT(handle.idx, statusbar_log::kMaxStatusbarHandles)
      << "Handle index should be less than statusbar_log::kMaxStatusbarHandles";

  err_code = statusbar_log::test::RedirectUpdateStatusbar(handle, 0, 50.0,
                                                          log_filename);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to update the status bar with valid handle";

  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle.valid)
      << "Handle should be marked as invalid after destruction";
}

TEST_F(HandleManagementTest, CreateMultiBarHandle) {
  statusbar_log::StatusbarHandle handle;
  std::vector<unsigned int> positions = {2, 1};
  std::vector<unsigned int> bar_sizes = {20, 10};
  std::vector<std::string> prefixes = {"first", "second"};
  std::vector<std::string> postfixes = {"20 long", "10 long"};

  const std::string log_filename = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename);

  EXPECT_NE(handle.id, 0) << "Handle should have a non-zero ID assigned";
  EXPECT_LT(handle.id, SIZE_MAX) << "Handle index should be a valid value";
  EXPECT_LT(handle.idx, statusbar_log::kMaxStatusbarHandles)
      << "Handle index should be less than statusbar_log::kMaxStatusbarHandles";
  err_code = statusbar_log::test::RedirectUpdateStatusbar(handle, 0, 50.0,
                                                          log_filename);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to update the status bar with valid handle";
  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle.valid)
      << "Handle should be marked as invalid after destruction";
}

TEST_F(HandleManagementTest, CreateHandle_InvalidInputSizes) {
  statusbar_log::StatusbarHandle handle;

  // Test Case 1: Positions vector larger than others
  {
    std::vector<unsigned int> positions = {1, 2};
    std::vector<unsigned int> bar_sizes = {50};
    std::vector<std::string> prefixes = {"Processing"};
    std::vector<std::string> postfixes = {"items"};

    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
        log_filename);

    EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should fail when positions vector (size " << positions.size()
        << ") is larger than others (sizes " << bar_sizes.size() << ","
        << prefixes.size() << "," << postfixes.size() << ")";

    EXPECT_FALSE(handle.valid)
        << "Handle should remain invalid after failed creation";
  }

  // Test Case 2: Bar sizes vector larger than others
  {
    std::vector<unsigned int> positions = {1};
    std::vector<unsigned int> bar_sizes = {50, 70};
    std::vector<std::string> prefixes = {"Processing"};
    std::vector<std::string> postfixes = {"items"};

    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
        log_filename);

    EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should fail when bar_sizes vector (size " << bar_sizes.size()
        << ") is larger than others (sizes " << positions.size() << ","
        << prefixes.size() << "," << postfixes.size() << ")";

    EXPECT_FALSE(handle.valid)
        << "Handle should remain invalid after failed creation";
  }

  // Test Case 3: prefixes vector larger than others
  {
    std::vector<unsigned int> positions = {1};
    std::vector<unsigned int> bar_sizes = {50};
    std::vector<std::string> prefixes = {"Processing", "more"};
    std::vector<std::string> postfixes = {"items"};

    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
        log_filename);

    EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should fail when prefixes vector (size " << prefixes.size()
        << ") is larger than others (sizes " << positions.size() << ","
        << bar_sizes.size() << "," << postfixes.size() << ")";

    EXPECT_FALSE(handle.valid)
        << "Handle should remain invalid after failed creation";
  }

  // Test Case 4: postfixes vector larger than others
  {
    std::vector<unsigned int> positions = {1};
    std::vector<unsigned int> bar_sizes = {50};
    std::vector<std::string> prefixes = {"Processing"};
    std::vector<std::string> postfixes = {"items", "more"};

    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
        log_filename);

    EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should fail when postfixes vector (size " << postfixes.size()
        << ") is larger than others (sizes " << positions.size() << ","
        << bar_sizes.size() << "," << prefixes.size() << ")";

    EXPECT_FALSE(handle.valid)
        << "Handle should remain invalid after failed creation";
  }
}

TEST_F(HandleManagementTest, CreateHandle_MaxActiveHandlesLimit) {
  std::vector<statusbar_log::StatusbarHandle> handles;
  handles.reserve(statusbar_log::kMaxStatusbarHandles + 5);  // Pre-allocate
  bool reached_limit = false;

  for (size_t i = 0; i < statusbar_log::kMaxStatusbarHandles + 5; ++i) {
    handles.emplace_back();
    auto& handle = handles.back();

    std::vector<unsigned int> positions = {1};
    std::vector<unsigned int> bar_sizes = {50};
    std::vector<std::string> prefixes = {"Test " + std::to_string(i)};
    std::vector<std::string> postfixes = {"item"};

    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
        log_filename);

    if (err_code == statusbar_log::kStatusbarLogSuccess) {
      // Successfully created - should only happen up to kMaxStatusbarHandles
      EXPECT_LE(handles.size(), statusbar_log::kMaxStatusbarHandles)
          << "Should not create more than "
          << statusbar_log::kMaxStatusbarHandles << " handles";
      EXPECT_TRUE(handle.valid);
    } else {
      // Should fail with specific error code
      EXPECT_EQ(err_code, -4)
          << "Should return error code -4 when max handles limit reached";
      EXPECT_FALSE(handle.valid)
          << "Handle should be invalid when creation fails";

      // Remove the failed handle from vector
      handles.pop_back();
      reached_limit = true;
      break;
    }
  }

  EXPECT_TRUE(reached_limit)
      << "Should have encountered the maximum handles limit";
  EXPECT_EQ(handles.size(), statusbar_log::kMaxStatusbarHandles)
      << "Should have created exactly " << statusbar_log::kMaxStatusbarHandles
      << " handles";

  // Test that we can still destroy handles and create new ones after cleanup
  if (!handles.empty()) {
    // Destroy one handle
    const std::string log_filename = GetTestLogFilename();
    int err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
        handles[0], log_filename);
    EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess);
    handles.erase(handles.begin());

    // Now we should be able to create one more handle
    handles.emplace_back();
    auto& new_handle = handles.back();
    std::vector<unsigned int> positions = {1};
    std::vector<unsigned int> bar_sizes = {50};
    std::vector<std::string> prefixes = {"New after destroy"};
    std::vector<std::string> postfixes = {"works"};

    err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        new_handle, this->sink_handle_, positions, bar_sizes, prefixes,
        postfixes, log_filename);

    EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should be able to create new handle after destroying one";
    EXPECT_TRUE(new_handle.valid);
  }

  // Clean up all handles
  for (auto& handle : handles) {
    const std::string log_filename = GetTestLogFilename();
    statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);
  }
}

TEST_F(HandleManagementTest, DestroyValidHandle) {
  statusbar_log::StatusbarHandle handle;
  std::vector<unsigned int> positions = {1};
  std::vector<unsigned int> bar_sizes = {50};
  std::vector<std::string> prefixes = {"Processing"};
  std::vector<std::string> postfixes = {"items"};

  const std::string log_filename = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename);

  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "CreateStatusbarHandle should return "
         "statusbar_log::kStatusbarLogSuccess";

  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle.valid)
      << "Handle should be marked as invalid after destruction";
}

TEST_F(HandleManagementTest, DestroyInvalidHandle) {
  statusbar_log::StatusbarHandle handle;
  std::vector<unsigned int> positions = {1};
  std::vector<unsigned int> bar_sizes = {50};
  std::vector<std::string> prefixes = {"Processing"};
  std::vector<std::string> postfixes = {"items"};

  const std::string log_filename = GetTestLogFilename();
  int err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should not be able to destroy a statusbar before it has been created";

  err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename);
  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "CreateStatusbarHandle should return "
         "statusbar_log::kStatusbarLogSuccess";

  std::size_t tmp_idx = handle.idx;
  handle.idx = SIZE_MAX;

  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
      << "If indexes don't match, destroying statusbars should not be possible";

  handle.idx = tmp_idx;
  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle.valid)
      << "Handle should be marked as invalid after destruction";
}

TEST_F(HandleManagementTest, DestroyAlreadyDestroyedHandle) {
  statusbar_log::StatusbarHandle handle;
  std::vector<unsigned int> positions = {1};
  std::vector<unsigned int> bar_sizes = {50};
  std::vector<std::string> prefixes = {"Processing"};
  std::vector<std::string> postfixes = {"items"};

  const std::string log_filename = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename);
  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "CreateStatusbarHandle should return "
         "statusbar_log::kStatusbarLogSuccess";

  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle.valid)
      << "Handle should be marked as invalid after destruction";

  err_code =
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle, log_filename);

  EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should not be able to destroy already destroyed handle";
}

// ==================================================
// StatusbarUpdateTest
// ==================================================

class StatusbarUpdateTest : public StatusbarTestBase {
 protected:
  statusbar_log::sink::SinkHandle sink_handle_;
  std::string log_filename_;
  statusbar_log::StatusbarHandle handle_;
  std::vector<unsigned int> positions_;
  std::vector<unsigned int> bar_sizes_;
  std::vector<std::string> prefixes_;
  std::vector<std::string> postfixes_;

  void SetUp() override {
    statusbar_log::sink::CreateSinkStdout(this->sink_handle_);
    this->positions_ = {1};
    this->bar_sizes_ = {50};
    this->prefixes_ = {"Processing"};
    this->postfixes_ = {"items"};
    this->log_filename_ = GetTestLogFilename();

    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        this->handle_, this->sink_handle_, this->positions_, this->bar_sizes_,
        this->prefixes_, this->postfixes_, this->log_filename_);

    ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Failed to create statusbar handle in fixture setup";
    ASSERT_TRUE(handle_.valid)
        << "Handle should be valid after creation in fixture setup";
  }

  void TearDown() override {
    // Only destroy if the handle is still valid
    if (handle_.valid) {
      int err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
          handle_, log_filename_);
      EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
          << "Failed to destroy statusbar handle in fixture teardown";
    }
    statusbar_log::sink::DestroySinkHandle(this->sink_handle_);
  }

  void UpdateBarAndVerify(size_t bar_index, double percentage) {
    int err_code = statusbar_log::test::RedirectUpdateStatusbar(
        this->handle_, bar_index, percentage, log_filename_);
    EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Failed to update bar at index " << bar_index << " with percentage "
        << percentage;
  }
  void updateBarAndVerifyFailure(size_t bar_index, double percentage) {
    int err_code = statusbar_log::test::RedirectUpdateStatusbar(
        this->handle_, bar_index, percentage, log_filename_);
    EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Should have failed to update bar with index " << bar_index
        << " with percentage " << percentage;
  }
};

TEST_F(StatusbarUpdateTest, UpdateValidPercentage) {
  UpdateBarAndVerify(0, 0.0);
  UpdateBarAndVerify(0, 25.5);
  UpdateBarAndVerify(0, 50.0);
  UpdateBarAndVerify(0, 75.0);
  UpdateBarAndVerify(0, 100.0);
}

TEST_F(StatusbarUpdateTest, UpdateBoundaryPercentages) {
  UpdateBarAndVerify(0, 0.0);
  UpdateBarAndVerify(0, 0.1);
  UpdateBarAndVerify(0, 99.9);
  UpdateBarAndVerify(0, 100.0);
}

TEST_F(StatusbarUpdateTest, UpdateMultipleBarsInHandle) {
  int err = statusbar_log::test::RedirectDestroyStatusbarHandle(
      this->handle_, this->log_filename_);
  ASSERT_EQ(err, statusbar_log::kStatusbarLogSuccess)
      << "Failed to destroy statusbar handle "
         "in UpdateMultipleBarsInHandle test";

  this->positions_ = {2, 1};
  this->bar_sizes_ = {50, 25};
  this->prefixes_ = {"second", "first"};
  this->postfixes_ = {"items", "things"};

  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      this->handle_, this->sink_handle_, this->positions_, this->bar_sizes_,
      this->prefixes_, this->postfixes_, this->log_filename_);

  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Failed to create multiple statusbars in UpdateMultipleBarsInHandle "
         "test";

  UpdateBarAndVerify(0, 0.0);
  UpdateBarAndVerify(1, 0.0);
  UpdateBarAndVerify(0, 25.5);
  UpdateBarAndVerify(0, 50.0);
  UpdateBarAndVerify(1, 50.0);
  UpdateBarAndVerify(0, 75.0);
  UpdateBarAndVerify(1, 75.0);
  UpdateBarAndVerify(0, 100.0);
  UpdateBarAndVerify(1, 100.0);
}

TEST_F(StatusbarUpdateTest, InvalidUpdates) {
  updateBarAndVerifyFailure(0, -1.0);
  updateBarAndVerifyFailure(0, 101.0);
  updateBarAndVerifyFailure(1, 100.0);

  statusbar_log::StatusbarHandle handle2;
  std::vector<unsigned int> positions2 = {1};
  std::vector<unsigned int> bar_sizes2 = {50};
  std::vector<std::string> prefixes2 = {"Processing"};
  std::vector<std::string> postfixes2 = {"items"};

  const std::string log_filename2 = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle2, this->sink_handle_, positions2, bar_sizes2, prefixes2,
      postfixes2, log_filename2);
  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "CreateStatusbarHandle should return "
         "statusbar_log::kStatusbarLogSuccess";

  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(handle2,
                                                                 log_filename2);

  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should be able to destroy the handle cleanly";
  EXPECT_FALSE(handle2.valid)
      << "Handle should be marked as invalid after destruction";

  err_code = statusbar_log::test::RedirectUpdateStatusbar(handle2, 0, 20,
                                                          log_filename2);

  EXPECT_NE(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Should not be able to destroy already destroyed handle";
}

// ==================================================
// StatusbarValidations
// ==================================================

class StatusbarValidations : public StatusbarTestBase {
 protected:
  std::string log_filename;
  statusbar_log::StatusbarHandle handle_;
  std::vector<unsigned int> positions_;
  std::vector<unsigned int> bar_sizes_;
  std::vector<std::string> prefixes_;
  std::vector<std::string> postfixes_;

  void SetUp() override {
    StatusbarTestBase::SetUp();
    this->positions_ = {1};
    this->bar_sizes_ = {50};
    this->prefixes_ = {"Processing"};
    this->postfixes_ = {"items"};
    this->log_filename = GetTestLogFilename();

    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        this->handle_, this->sink_handle_, this->positions_, this->bar_sizes_,
        this->prefixes_, this->postfixes_, this->log_filename);

    ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Failed to create statusbar handle in fixture setup";
    ASSERT_TRUE(handle_.valid)
        << "Handle should be valid after creation in fixture setup";
  }
  void TearDown() override {
    if (handle_.valid) {
      statusbar_log::test::RedirectDestroyStatusbarHandle(handle_,
                                                          log_filename);
    }
    StatusbarTestBase::TearDown();
  }
};

TEST_F(StatusbarValidations, IsValidHandle) {
  statusbar_log::StatusbarHandle handle2;
  std::vector<unsigned int> positions = {1};
  std::vector<unsigned int> bar_sizes = {50};
  std::vector<std::string> prefixes = {"Processing"};
  std::vector<std::string> postfixes = {"items"};

  std::string log_filename2 = GetTestLogFilename();
  int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
      handle2, this->sink_handle_, positions, bar_sizes, prefixes, postfixes,
      log_filename2);
  ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
      << "Failed to create statusbar handle";
  ASSERT_TRUE(handle2.valid) << "Handle should be valid after creation";

  handle_.valid = false;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -1);

  handle_.valid = true;
  int idx_backup = handle_.idx;
  handle_.idx = SIZE_MAX;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -2);

  handle_.idx = 99999;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -2);

  handle_.idx = idx_backup + 1;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -3);

  handle_.idx = idx_backup;
  int ID_backup = handle_.id;
  handle_.id = handle2.id;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -3);

  handle_.id = ID_backup + 1;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, -3);

  // BUG : Cant test err_code -4

  handle_.id = ID_backup;
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
      handle_, this->log_filename);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess);
  err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(handle2,
                                                                 log_filename2);
  EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess);
}

// ==================================================
// Log validations
// ==================================================

class LogTest : public StatusbarTestBase {
 protected:
  statusbar_log::sink::SinkHandle sink_handle_;
  void SetUp() override {
    statusbar_log::sink::CreateSinkStdout(this->sink_handle_);
    ASSERT_EQ(statusbar_log::kLogLevel, statusbar_log::kLogLevelInf)
        << "Ensure tests are compiled with kLogLevelInf, otherwise some tests "
           "will fail!";
  }
  void TearDown() override {
    statusbar_log::sink::DestroySinkHandle(this->sink_handle_);
  }
};

TEST_F(LogTest, LogLevelsTest) {
  std::string capture_stdout;

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelDbg, kFilename,
                                        "Debug Test");

  EXPECT_STREQ(capture_stdout.c_str(), "")
      << "Expected output of LogDbg did not match actual output (should be no "
         "output as log level is below threshhold)";

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "Info Test");

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: Info Test\n")
      << "Expected output of LogInf did not match actual output";

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelWrn, kFilename,
                                        "Warn Test");

  EXPECT_STREQ(capture_stdout.c_str(),
               "WARNING [statusbarlog_test.cc]: Warn Test\n")
      << "Expected output of LogWrn did not match actual output";

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelErr, kFilename,
                                        "Error Test");

  EXPECT_STREQ(capture_stdout.c_str(),
               "ERROR [statusbarlog_test.cc]: Error Test\n")
      << "Expected output of LogErr did not match actual output";
}

TEST_F(LogTest, LogFormatStringTest) {
  std::string capture_stdout;

  statusbar_log::test::RedirectToStrLog(
      capture_stdout, this->sink_handle_, statusbar_log::kLogLevelInf,
      kFilename,
      "int: %i, unsigned: %u, hex: %#x, oct: %#o, short: %hd, long: %ld, long "
      "long: %lld",
      -1, 2u, 255u, 8u, (short)3, -1234567890L, 1234567890123LL);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "int: -1, unsigned: 2, hex: 0xff, oct: 010, short: 3, "
               "long: -1234567890, long long: 1234567890123"
               "\n");

  statusbar_log::test::RedirectToStrLog(
      capture_stdout, this->sink_handle_, statusbar_log::kLogLevelInf,
      kFilename, "size_t: %zu, ssize_t: %zd", (size_t)42, (ssize_t)-5);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "size_t: 42, ssize_t: -5"
               "\n");

  statusbar_log::test::RedirectToStrLog(
      capture_stdout, this->sink_handle_, statusbar_log::kLogLevelInf,
      kFilename, "unsigned (wrap): %u", (unsigned int)(-1));

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "unsigned (wrap): 4294967295"
               "\n");

  double v = 1234.56789;

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "v %%f: %f", v);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "v %f: 1234.567890"
               "\n");

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "v %%.2f: %.2f", v);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "v %.2f: 1234.57"
               "\n");

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "v %%+09.2f: %+09.2f", v);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "v %+09.2f: +01234.57"
               "\n");

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "v %%e: %e, v %%E: %E", v, v);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "v %e: 1.234568e+03, v %E: 1.234568E+03"
               "\n");

  long double w = 1.234567890123456789L;

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "long double Lf: %Lf", w);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "long double Lf: 1.234568"
               "\n");

  std::string s = "hello";
  const char* cs = "chars";
  char c = 'A';

  statusbar_log::test::RedirectToStrLog(
      capture_stdout, this->sink_handle_, statusbar_log::kLogLevelInf,
      kFilename, "str: %s, c_str: %s, char: %c", s.c_str(), cs, c);

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "str: hello, c_str: chars, char: A"
               "\n");

  statusbar_log::test::RedirectToStrLog(
      capture_stdout, this->sink_handle_, statusbar_log::kLogLevelInf,
      kFilename, "[%10s] [%-10s] [%.3s]", "hi", "left", "truncate");

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "[        hi] [left      ] [tru]"
               "\n");

  statusbar_log::test::RedirectToStrLog(capture_stdout, this->sink_handle_,
                                        statusbar_log::kLogLevelInf, kFilename,
                                        "int:%i u:%u sz:%zu f:%.2f s:%s c:%c",
                                        -1, 2u, (size_t)7, 3.14159, "ok", 'Z');

  EXPECT_STREQ(capture_stdout.c_str(),
               "INFO [statusbarlog_test.cc]: "
               "int:-1 u:2 sz:7 f:3.14 s:ok c:Z"
               "\n");
}

class ReadStatusbarUpdateTest : public StatusbarTestBase {
 protected:
  statusbar_log::sink::SinkHandle sink_handle_;
  std::string log_filename_;
  statusbar_log::StatusbarHandle handle_;
  std::vector<unsigned int> positions_;
  std::vector<unsigned int> bar_sizes_;
  std::vector<std::string> prefixes_;
  std::vector<std::string> postfixes_;

  void SetUp() override {
    statusbar_log::sink::CreateSinkStdout(this->sink_handle_);
    ASSERT_EQ(statusbar_log::kLogLevel, statusbar_log::kLogLevelInf)
        << "Ensure tests are compiled with kLogLevelInf, otherwise some tests "
           "will fail!";
    this->positions_ = {1};
    this->bar_sizes_ = {50};
    this->prefixes_ = {"Processing"};
    this->postfixes_ = {"items"};
    this->log_filename_ = GetTestLogFilename();

    int err_code = statusbar_log::test::RedirectCreateStatusbarHandle(
        this->handle_, this->sink_handle_, this->positions_, this->bar_sizes_,
        this->prefixes_, this->postfixes_, this->log_filename_);

    ASSERT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
        << "Failed to create statusbar handle in fixture setup";
    ASSERT_TRUE(handle_.valid)
        << "Handle should be valid after creation in fixture setup";
  }

  void TearDown() override {
    // Only destroy if the handle is still valid
    if (handle_.valid) {
      int err_code = statusbar_log::test::RedirectDestroyStatusbarHandle(
          handle_, log_filename_);
      EXPECT_EQ(err_code, statusbar_log::kStatusbarLogSuccess)
          << "Failed to destroy statusbar handle in fixture teardown";
    }
    statusbar_log::sink::DestroySinkHandle(this->sink_handle_);
  }
};

// TEST_F(ReadStatusbarUpdateTest, SimpleStatusbar) {
//   std::string capture_stdout;
//   statusbar_log::test::RedirectToStrUpdateStatusbar(capture_stdout,
//                                                     this->handle_, 0, 50);
//   EXPECT_STREQ(capture_stdout.c_str(),
//                "INFO [statusbarlog_test.cc]: "
//                "int:-1 u:2 sz:7 f:3.14 s:ok c:Z"
//                "\n");
// }
//
// ==================================================
// Main function with test directory management
// ==================================================

int main(int argc, char** argv) {
  statusbar_log::test::SetupTestOutputDirectory();

  ::testing::InitGoogleTest(&argc, argv);

  int result = RUN_ALL_TESTS();

  return result;
}
