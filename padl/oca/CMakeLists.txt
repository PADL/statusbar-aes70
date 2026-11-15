# Copyright 2026 PADL Software Pty Ltd
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  oca
  SOURCES
  oca_aes67.cpp
  DEPS
  statusbar::ocp1
  statusbar::buffer
  statusbar::status
  TESTS
  oca_aes67_test.cpp
  oca_types_test.cpp)
