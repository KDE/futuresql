# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
#
# SPDX-License-Identifier: BSD-2-Clause

include_directories(${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/src)

add_executable(hello-world hello-world.cpp)
add_executable(transaction transaction.cpp)

target_link_libraries(hello-world Qt::Core QCoro5::Core futuresql${QT_MAJOR_VERSION})
target_link_libraries(transaction Qt::Core QCoro5::Core futuresql${QT_MAJOR_VERSION})
