# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im
#
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.19)

project(futuresql LANGUAGES CXX VERSION "0.1.0")

option(BUILD_EXAMPLES "Build examples" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_BINARY_DIR ON)

set(REQUIRED_QT_VERSION 5.15.2)

find_package(ECM 5.93.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(ECMSetupVersion)
include(ECMGenerateExportHeader)
include(ECMGenerateHeaders)

find_package(Qt${QT_MAJOR_VERSION} ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE COMPONENTS Core Sql)
set(CMAKE_AUTOMOC ON)

add_subdirectory(src)
if (BUILD_EXAMPLES)
    find_package(QCoro REQUIRED)
    qcoro_enable_coroutines()
    add_subdirectory(examples)
endif()
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
