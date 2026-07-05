# SPDX-License-Identifier: Apache-2.0 Copyright (c) 2025 Lukas Widmer

# -- statusbarLog/tests/CMakeLists.txt

# =============================================================================
# Google Test
# =============================================================================
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
      DOWNLOAD_EXTRACT_TIMESTAMP
      FALSE)

# Only set gtest_force_shared_crt on Windows
if(WIN32)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(googletest)

add_executable(${PROJECT_NAME}_test
               ${CMAKE_CURRENT_SOURCE_DIR}/src/statusbarlog_test.cc
               ${CMAKE_CURRENT_SOURCE_DIR}/src/statusbarlog_test_helpers.cc)

target_include_directories(
  ${PROJECT_NAME}_test
  PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
          $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(${PROJECT_NAME}_test PUBLIC cxx_std_20)
# strict ISO C++20: ISO dialect (-std=c++20, not gnu++20) + extensions are errors
set_target_properties(${PROJECT_NAME}_test PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(${PROJECT_NAME}_test PRIVATE -std=c++20 -pedantic-errors)

target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME}
                                                   GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)

# =============================================================================
# Main Testing Executable (Optional)
# =============================================================================

if(STATUSBARLOG_BUILD_TEST_MAIN)
  add_executable(${PROJECT_NAME}_main
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/statusbarlog_main.cc)

  target_include_directories(
    ${PROJECT_NAME}_main
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>)

  target_link_libraries(${PROJECT_NAME}_main PRIVATE ${PROJECT_NAME})
endif()
