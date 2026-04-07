cmake_minimum_required(VERSION 3.16.3)
project(libqemu VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Enabled QEMU targets
if(NOT DEFINED LIBQEMU_TARGETS)
    set(LIBQEMU_TARGETS aarch64)
endif()

# For the arm target, use the aarch64 target which is a superset of the former.
set(temp)
foreach(target ${LIBQEMU_TARGETS})
    if(target STREQUAL "arm")
        set(target aarch64)
    endif()
    list(APPEND temp ${target})
endforeach()
set(LIBQEMU_TARGETS ${temp})

if(LIST_LIBQEMU_TARGETS)
    message(${LIBQEMU_TARGETS})
    return()
endif()

set(LIBQEMU_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR}/libqemu)

include(qemu.cmake)

# --- Target ---

add_library(libqemu INTERFACE)
add_library(libqemu::libqemu ALIAS libqemu)
add_dependencies(libqemu qemu)
target_include_directories(libqemu INTERFACE
    $<BUILD_INTERFACE:${QEMU_INSTALL_DIR}/${LIBQEMU_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${LIBQEMU_INCLUDE_DIR}>)

# --- Package config ---

set(LIBQEMU_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/libqemu)
set(LIBQEMU_CONFIG_FILE libqemuConfig.cmake)
set(LIBQEMU_VERSION_FILE libqemuConfigVersion.cmake)

configure_package_config_file(
    ${LIBQEMU_CONFIG_FILE}.in
    ${CMAKE_CURRENT_BINARY_DIR}/${LIBQEMU_CONFIG_FILE}
    INSTALL_DESTINATION ${LIBQEMU_CMAKE_DIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${LIBQEMU_VERSION_FILE}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

export(EXPORT libqemu-targets NAMESPACE libqemu::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/libqemu-targets.cmake)

# --- Install ---

install(TARGETS libqemu EXPORT libqemu-targets)

install(EXPORT libqemu-targets
    NAMESPACE libqemu::
    DESTINATION ${LIBQEMU_CMAKE_DIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LIBQEMU_CONFIG_FILE}
              ${CMAKE_CURRENT_BINARY_DIR}/${LIBQEMU_VERSION_FILE}
    DESTINATION ${LIBQEMU_CMAKE_DIR})
