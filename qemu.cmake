include(ExternalProject)

find_package(Python COMPONENTS Interpreter REQUIRED)

set(QEMU_CONF_ARGS
    -Dlibqemu=true
    -Db_staticpic=true
    --disable-debug-tcg
    --disable-sparse
    --enable-sdl
    --enable-vnc
    --enable-vnc-jpeg
    --disable-vnc-sasl
    --disable-xen
    --disable-brlapi
    --disable-png
    --disable-curses
    --disable-curl
    --disable-user
    --disable-linux-user
    --disable-bsd-user
    --enable-pie
    --disable-linux-aio
    --disable-attr
    --disable-install-blobs
    --disable-docs
    --disable-vhost-net
    --disable-spice
    --disable-usb-redir
    --disable-guest-agent
    --disable-cap-ng
    --disable-libiscsi
    --disable-libusb
    --disable-tools
    --disable-nettle
    --disable-virglrenderer
    --disable-virclrenderer
    --disable-virqnnrenderer
    --enable-opengl
    --enable-slirp
    --disable-vde
    --disable-vte
    --disable-rbd
    --disable-smartcard
    --disable-libnfs
    --disable-snappy
    --disable-numa
    --disable-glusterfs
    --audio-drv-list=
    --disable-werror
    --disable-capstone
    --enable-kvm
    --enable-vhost-user
)
set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --libdir=lib)

if (GS_ENABLE_VIRCLRENDERER)
    if (WIN32)
        message(FATAL_ERROR "libqemu: virclrenderer is not supported on Windows")
    endif()

    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-virclrenderer)
    list(APPEND QEMU_DEPENDENCIES virclrenderer)
endif()

if (GS_ENABLE_VIRQNNRENDERER)
    message(STATUS "libqemu: enabling virqnnrender")
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-virqnnrenderer)
    list(APPEND QEMU_DEPENDENCIES virqnnrenderer qnn_sdk_path)
endif()

if (GS_ENABLE_VIRGLRENDERER)
    message(STATUS "libqemu: enabling virglrender")
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-virglrenderer)
endif()

# may be un-necissary in future releases of QEMU?
if (APPLE)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS}
        --enable-hvf
        --disable-strip
        --disable-pie
        --disable-gtk
        --disable-sdl-image
        --disable-kvm
    )
elseif(WIN32)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS}
        --disable-kvm
        --disable-pie
        --disable-vhost-user
    )
else()
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --extra-ldflags=-lrt)
endif()

if (GS_ENABLE_CAPSTONE)
    message(STATUS "libqemu: enabling capstone dependency")
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-capstone)
endif()
set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --cc=${CMAKE_C_COMPILER})
set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --cxx=${CMAKE_CXX_COMPILER})
# QEMU compiles some helper programs to run at building time. Set this to
# your host compiler if you are cross-compiling libqemu to another platform.
if(LIBQEMU_HOST_CC)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --host-cc=${LIBQEMU_HOST_CC})
endif()

if (GS_ENABLE_SANITIZERS)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-asan --enable-ubsan)
endif()

if (GS_ENABLE_LLD)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --extra-ldflags=-fuse-ld=lld)
endif()

if (GS_ENABLE_LTO)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-lto)
endif()

if(QEMU_ENABLE_USB_REDIRECT)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS}
        --enable-usb-redir
        --enable-libusb
     )
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-debug --enable-debug-tcg --enable-debug-info)
elseif(CMAKE_BUILD_TYPE STREQUAL "RELWITHDEBINFO")
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-debug-info)
endif()

set(cflags "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
set(cxxflags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
set(ldflags "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_${CMAKE_BUILD_TYPE}}")

if(${CMAKE_CXX_STANDARD})
    set(cxxflags "-std=c++${CMAKE_CXX_STANDARD} ${cxxflags}")
endif()

# Detect if running under MSYS2 (UCRT64 or MINGW64) on Windows ARM64
# and set up cross-compilation to x86_64 accordingly.
# Reference:
#   https://www.qemu.org/docs/master/devel/build-environment.html#build-on-windows-aarch64
if(DEFINED ENV{MSYSTEM} AND ("$ENV{MSYSTEM}" MATCHES "UCRT64|MINGW64"))
    # There is not reliable CMake variable to detect the host architecture on Windows,
    # so we use a PowerShell command to query the Win32_Processor WMI class
    execute_process(
        COMMAND powershell -NoProfile -Command "(Get-WmiObject Win32_Processor).Architecture"
        OUTPUT_VARIABLE WIN_PROC_ARCH_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Architecture code 12 = ARM64
    if(WIN_PROC_ARCH_CODE EQUAL "12")
        message(STATUS "Detected ARM64 Windows with MSYS2 $ENV{MSYSTEM} - configuring for x86_64 cross-compilation")
        set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --cpu=x86_64 --cross-prefix=)
    endif()
endif()


# With cmake 3.12 we could use the list(TRANSFORM ...) operator
function(list_prefix_suffix var prefix suffix)
    set(tmp)
    foreach(item ${${var}})
        list(APPEND tmp ${prefix}${item}${suffix})
    endforeach()
    set(var ${tmp} PARENT_SCOPE)
endfunction()

set(target_list "")
foreach(target ${LIBQEMU_TARGETS})
    if(target_list)
        set(target_list "${target_list},${target}-softmmu")
    else()
        set(target_list "${target}-softmmu")
    endif()
endforeach()

# QEMU's own build parallelizes via meson/ninja. When the top-level project is
# built with GNU Make, a literal $(MAKE) in the recipe makes Make treat this as
# a recursive sub-make and forward its jobserver, so QEMU honours the user's -j.
if(CMAKE_GENERATOR MATCHES "Make")
    set(_qemu_build_command "$(MAKE)")
else()
    set(_qemu_build_command ${CMAKE_MAKE_PROGRAM})
endif()

ExternalProject_Add(qemu
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ${CONFIGURE_ENVIRONMENT_VARIABLE} ${CMAKE_CURRENT_SOURCE_DIR}/configure
        '--extra-ldflags=${ldflags}'
        '--extra-cflags=${cflags}'
        '--extra-cxxflags=${cxxflags}'
        --prefix=<INSTALL_DIR>
        --target-list=${target_list}
        ${QEMU_CONF_ARGS}
    BUILD_COMMAND ${_qemu_build_command}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    BUILD_ALWAYS on
)

ExternalProject_Get_Property(qemu INSTALL_DIR)
set(QEMU_INSTALL_DIR ${INSTALL_DIR})

install(DIRECTORY ${QEMU_INSTALL_DIR}/${LIBQEMU_INCLUDE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${QEMU_INSTALL_DIR}/share/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})

# Populate qemu share directory in build tree.
# This ensures libqemu can load keymaps during the development phase
add_custom_command(TARGET qemu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${QEMU_INSTALL_DIR}/share
    ${CMAKE_BINARY_DIR}/share)

foreach(target ${LIBQEMU_TARGETS})
    set(lib_name libqemu-system-${target}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(lib_path ${QEMU_INSTALL_DIR}/lib/${lib_name})

    target_compile_definitions(libqemu INTERFACE
        LIBQEMU_TARGET_${target}_LIBRARY="${lib_name}")

    if (WIN32)
        install(FILES ${lib_path} DESTINATION ${CMAKE_INSTALL_BINDIR})
        add_custom_command(TARGET qemu POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${lib_path}
            ${CMAKE_BINARY_DIR})
    else()
        install(FILES ${lib_path} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endforeach()

# Windows' DLL loader does not search libdir, so libqemu plugin DLLs must
# sit next to libqemu-system-*.dll for g_module_open to find them.
if (WIN32)
    set(LIBQEMU_PLUGINS libidlinker)
    foreach(plugin ${LIBQEMU_PLUGINS})
        set(plugin_path ${QEMU_INSTALL_DIR}/lib/${plugin}.dll)
        install(FILES ${plugin_path} DESTINATION ${CMAKE_INSTALL_BINDIR})
        add_custom_command(TARGET qemu POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${plugin_path} ${CMAKE_BINARY_DIR})
    endforeach()
endif()
