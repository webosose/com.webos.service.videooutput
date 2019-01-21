# Copyright (c) 2016-2019 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 2.8.7)

project(videooutputd CXX)
include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC7)
webos_component(1 0 0)

set(BIN_NAME videooutputd)

webos_add_compiler_flags(ALL -std=c++11 -Wall -Wextra -fno-permissive)
#disable specific warnings
webos_add_compiler_flags(ALL -Wno-unused-parameter -Wno-deprecated-declarations -Wno-type-limits -Wno-comment)
#promote specific warnings to errors
webos_add_compiler_flags(ALL -Werror=return-type  -Werror=reorder -Werror=uninitialized)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2++)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS})

pkg_check_modules(LS2HELPERS REQUIRED ls2-helpers)
include_directories(${LS2HELPERS_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LS2HELPERS_CFLAGS})

pkg_check_modules(PBNJSON_CXX REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CXX_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CXX_CFLAGS_OTHER})

pkg_check_modules(PMLOG REQUIRED PmLogLib)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS_OTHER})

pkg_check_modules(VAL_API REQUIRED videooutput-adaptation-layer-api)
include_directories(${VAL_API_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${VAL_API_CFLAGS_OTHER})

pkg_check_modules(VAL_IMPL REQUIRED val-impl)
include_directories(${VAL_IMPL_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${VAL_IMPL_CFLAGS_OTHER})

include_directories(${PROJECT_SOURCE_DIR}/src/common ${PROJECT_SOURCE_DIR}/src/subscribe ${PROJECT_SOURCE_DIR}/src/systemproperty ${PROJECT_SOURCE_DIR}/src/video)

if("${WEBOS_TARGET_DISTRO}" STREQUAL "webos")
    set(START_ON_SIGNAL "lginit_app-ready and stopped dbus")
endif()

if("${WEBOS_TARGET_MACHINE}" MATCHES "raspberrypi*")
    message(STATUS "Using ${WEBOS_TARGET_MACHINE} machine")
    add_definitions(-DUSE_RPI_RESOURCE)
    add_definitions(-DUMS_INTERNAL_API_VERSION2)
    set(ARC_SOURCE aspectratiocontrol.cpp)
else()
# case for qemux86 build
    message(STATUS "Using unknown machine")
    set(ARC_SOURCE aspectratiocontrol.cpp)
endif()

file(GLOB SOURCE_FILES
    src/common/errors.cpp
    src/video/${ARC_SOURCE}
    src/video/videoinfotypes.cpp
    src/video/videoservice.cpp
    src/video/videoservicetypes.cpp
    src/subscribe/aspectratiosetting.cpp
    src/subscribe/picturemode.cpp
    src/subscribe/picturesettings.cpp
    src/systemproperty/systempropertyservice.cpp
    src/main.cpp
    )

webos_add_linker_options(ALL --no-undefined)

#add local include directories, accessible only by double quotes include.
webos_add_compiler_flags(ALL -iquote ${CMAKE_CURRENT_BINARY_DIR}/Configured/files/conf)
webos_add_compiler_flags(ALL -iquote ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(${BIN_NAME} ${SOURCE_FILES})

target_link_libraries(${BIN_NAME}
        ${VAL_IMPL_LDFLAGS}
        ${GLIB2_LDFLAGS}
        ${LUNASERVICE2_LDFLAGS}
        ${LS2HELPERS_LDFLAGS}
        ${PBNJSON_CXX_LDFLAGS}
        ${PMLOG_LDFLAGS}
        rt
        pthread)

webos_build_daemon()
webos_build_system_bus_files()
webos_build_configured_file(files/conf/pmlog/videooutputd.conf SYSCONFDIR pmlog.d)

add_subdirectory(tests)
