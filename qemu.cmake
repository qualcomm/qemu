include(ExternalProject)

find_package(PythonInterp REQUIRED)

set(QEMU_CONF_ARGS
    --disable-debug-tcg
    --disable-sparse
    --enable-sdl
    --enable-vnc
    --disable-xen
    --disable-brlapi
    --disable-vnc-sasl
    --disable-vnc-jpeg
    --disable-png
    --disable-curses
    --disable-curl
    --disable-kvm
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
    --enable-virglrenderer
    --enable-opengl
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
)

# may be un-necissary in future releases of QEMU?
if (APPLE)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS}
        --disable-strip
        --disable-pie
        --disable-gtk
	--disable-sdl-image)
endif()

set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --cc=${CMAKE_C_COMPILER})
set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --cxx=${CMAKE_CXX_COMPILER})
# QEMU compiles some helper programs to run at building time. Set this to
# your host compiler if you are cross-compiling libqemu to another platform.
if(LIBQEMU_HOST_CC)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --host-cc=${LIBQEMU_HOST_CC})
endif()

if (GS_ENABLE_SANITIZERS)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-sanitizers)
endif()

if (GS_ENABLE_LLD)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --extra-ldflags=-fuse-ld=lld)
endif()

if (GS_ENABLE_LTO)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-lto)
endif()

if(GS_ENABLE_TSAN)
    set(QEMU_CONF_ARGS ${QEMU_CONF_ARGS} --enable-tsan)
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

ExternalProject_Add(qemu
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure
        '--extra-ldflags=${ldflags}'
        '--extra-cflags=${cflags}'
        '--extra-cxxflags=${cxxflags}'
        --prefix=<INSTALL_DIR>
        --enable-libqemu
        --target-list=${target_list}
        ${QEMU_CONF_ARGS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    BUILD_ALWAYS on
)

ExternalProject_Get_Property(qemu INSTALL_DIR)
set(QEMU_INSTALL_DIR ${INSTALL_DIR})

install(DIRECTORY ${QEMU_INSTALL_DIR}/${LIBQEMU_INCLUDE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(WIN32)
    set(LIBEXE "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.23.28105/bin/Hostx64/x64/lib.exe")
    foreach(target ${LIBQEMU_TARGETS})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qemu-prefix/src/qemu-build/${target}-softmmu/libqemu-system-${target}.dll DESTINATION lib)
        set(DEF "${CMAKE_CURRENT_BINARY_DIR}/qemu-prefix/src/qemu-build/${target}-softmmu/libqemu-system-${target}.def")
        install(CODE "execute_process(COMMAND \"${LIBEXE}\" /machine:x64 /NOLOGO /def:${DEF})")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libqemu-system-${target}.lib DESTINATION lib)
        install(CODE "execute_process(
            COMMAND ldd ${CMAKE_CURRENT_BINARY_DIR}/qemu-prefix/src/qemu-build/${target}-softmmu/libqemu-system-${target}.dll
            COMMAND grep \"/mingw64\"
            COMMAND cut -d' ' -f 3
            COMMAND xargs -n1 -IDLL cp -v DLL ${CMAKE_INSTALL_PREFIX}/lib
        )")
    endforeach()
else()
    foreach(target ${LIBQEMU_TARGETS})
        add_library(libqemu-${target} INTERFACE)

        if(APPLE)
            set(lib_name libqemu-system-${target}.dylib)
        else()
            set(lib_name libqemu-system-${target}.so)
        endif()
        set(lib_path ${QEMU_INSTALL_DIR}/lib/${lib_name})

        target_link_libraries(libqemu-${target} INTERFACE ${lib_name})

        install(FILES ${lib_path} DESTINATION ${LIBQEMU_LIB_DIR})
        install(TARGETS libqemu-${target} DESTINATION ${LIBQEMU_LIB_DIR} EXPORT libqemu-targets)
    endforeach()
endif()
