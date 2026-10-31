# Copyright 2026 PADL Software Pty Ltd
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  ocp1
  SOURCES
  ocp1_error.cpp
  ocp1_aes67.cpp
  ocp1_codec.cpp
  ocp1_pdu.cpp
  DEPS
  statusbar::buffer
  statusbar::ieee
  statusbar::status
  TESTS
  ocp1_aes67_test.cpp
  ocp1_codec_test.cpp
  ocp1_pdu_test.cpp)
