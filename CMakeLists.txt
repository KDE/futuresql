# SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
#
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.19)

project(futuresql LANGUAGES CXX VERSION "0.1.0")

option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_TESTING "Build tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_BINARY_DIR ON)

set(REQUIRED_QT_VERSION 6.6)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(FeatureSummary)
include(CTest)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMSetupVersion)
include(ECMGenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMAddTests)

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE COMPONENTS Core Sql)

add_subdirectory(src)
if(BUILD_TESTING OR BUILD_EXAMPLES)
    find_package(QCoro6 REQUIRED COMPONENTS Core)
    qcoro_enable_coroutines()
endif()

if(BUILD_TESTING)
    find_package(Qt6Test REQUIRED)
    add_subdirectory(tests)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
