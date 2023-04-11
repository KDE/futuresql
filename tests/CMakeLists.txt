# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
#
# SPDX-License-Identifier: BSD-2-Clause

ecm_add_tests(sqlitetest.cpp LINK_LIBRARIES futuresql${QT_MAJOR_VERSION} QCoro${QT_MAJOR_VERSION}::Core Qt::Test)
include_directories(${CMAKE_SOURCE_DIR}/src/ ${CMAKE_BINARY_DIR}/src/)
target_compile_definitions(sqlitetest PRIVATE -DTEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
