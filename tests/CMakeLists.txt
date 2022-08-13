# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
#
# SPDX-License-Identifier: BSD-2-Clause

ecm_add_tests(sqlitetest.cpp LINK_LIBRARIES futuresql QCoro5::Core Qt::Test)
include_directories(${CMAKE_SOURCE_DIR}/src/ ${CMAKE_BINARY_DIR}/src/)
