# Copyright 2026 PADL Software Pty Ltd
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  uuid
  SOURCES
  uuid_sha1.cpp
  uuid_value.cpp
  DEPS
  statusbar::buffer
  statusbar::ieee
  TESTS
  uuid_sha1_test.cpp
  uuid_value_test.cpp)
