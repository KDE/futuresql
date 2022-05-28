add_library(futuresql SHARED threadeddatabase.cpp)

target_link_libraries(futuresql PUBLIC Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Sql)

add_library(FutureSQL::FutureSQL ALIAS futuresql)

set(FUTURESQL_INSTALL_INCLUDEDIR "${KDE_INSTALL_INCLUDEDIR}/FutureSQL")

target_include_directories(futuresql
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE "$<INSTALL_INTERFACE:${FUTURESQL_INSTALL_INCLUDEDIR}>"
)

set_target_properties(futuresql PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   0
    EXPORT_NAME "FutureSQL"
)

ecm_generate_export_header(futuresql
    VERSION ${PROJECT_VERSION}
    BASE_NAME FutureSQL
)

install(TARGETS futuresql
    EXPORT FutureSQLTargets
    ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

set(FutureSQL_HEADERS threadeddatabase.h)

ecm_generate_headers(FutureSQL_CAMEL_CASE_HEADERS
    HEADER_NAMES
    ThreadedDatabase

    REQUIRED_HEADERS
    ${FutureSQL_HEADERS}
)

set(FutureSQL_HEADERS ${FutureSQL_HEADERS} threadeddatabase_p.h ${CMAKE_CURRENT_BINARY_DIR}/futuresql_export.h)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/FutureSQL")

ecm_setup_version(
  PROJECT
  VARIABLE_PREFIX FutureSQL
  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/futuresql_version.h"
  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/FutureSQLConfigVersion.cmake"
  SOVERSION 5)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/FutureSQLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FutureSQLConfig.cmake"
    INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
    PATH_VARS  CMAKE_INSTALL_PREFIX
)

install(FILES ${FutureSQL_HEADERS}
        DESTINATION ${FUTURESQL_INSTALL_INCLUDEDIR} # prefix matching C++ namespace
        COMPONENT Devel)
install(FILES ${FutureSQL_CAMEL_CASE_HEADERS}
        DESTINATION ${FUTURESQL_INSTALL_INCLUDEDIR} # prefix matching C++ namespace
        COMPONENT Devel)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/FutureSQLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/FutureSQLConfigVersion.cmake"
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Devel
)

install(EXPORT FutureSQLTargets
    DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
    FILE FutureSQLTargets.cmake
    NAMESPACE FutureSQL::
)
