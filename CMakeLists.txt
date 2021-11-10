cmake_minimum_required(VERSION 3.3)
project(libqemu)

include(GNUInstallDirs)

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
set(LIBQEMU_LIB_DIR ${CMAKE_INSTALL_LIBDIR}/libqemu)

include(qemu.cmake)

add_library(libqemu INTERFACE)
add_dependencies(libqemu qemu)

target_include_directories(libqemu INTERFACE
    $<BUILD_INTERFACE:${QEMU_INSTALL_DIR}/${LIBQEMU_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${LIBQEMU_INCLUDE_DIR}>)

install(TARGETS libqemu DESTINATION ${LIBQEMU_LIB_DIR} EXPORT libqemu-targets)
install(EXPORT libqemu-targets DESTINATION ${LIBQEMU_LIB_DIR}/cmake)

export(EXPORT libqemu-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/libqemu-targets.cmake)

export(PACKAGE libqemu)

include(CMakePackageConfigHelpers)

configure_package_config_file(libqemuConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libqemuConfig.cmake
    INSTALL_DESTINATION ${LIBQEMU_LIB_DIR}/cmake
    PATH_VARS LIBQEMU_LIB_DIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libqemuConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libqemuConfig.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/libqemuConfigVersion.cmake
    DESTINATION ${LIBQEMU_LIB_DIR}/cmake)
