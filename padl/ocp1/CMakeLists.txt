# Copyright 2026 PADL Software Pty Ltd
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  ocp1
  SOURCES
  ocp1_error.cpp
  ocp1_codec.cpp
  ocp1_pdu.cpp
  DEPS
  statusbar::buffer
  statusbar::ieee
  statusbar::status
  TESTS
  ocp1_codec_test.cpp
  ocp1_pdu_test.cpp)

# Fuzz harnesses (built only when -DENABLE_FUZZING=ON).
add_aes70_fuzzer(${CMAKE_CURRENT_LIST_DIR}/ocp1_codec_fuzzer.cpp)
add_aes70_fuzzer(${CMAKE_CURRENT_LIST_DIR}/ocp1_pdu_fuzzer.cpp)
