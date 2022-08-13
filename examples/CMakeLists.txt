# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
#
# SPDX-License-Identifier: BSD-2-Clause

add_executable(hello-world hello-world.cpp)

target_link_libraries(hello-world QCoro5::Core futuresql)
target_include_directories(hello-world PUBLIC ${CMAKE_BINARY_DIR}/src ${CMAKE_SOURCE_DIR}/src)
