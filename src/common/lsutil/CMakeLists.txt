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

project(ls2-helpers)

include(FindPkgConfig)

include_directories(src)
include_directories(include/public/ls2-helpers)

webos_add_compiler_flags(ALL -fPIC)

# -- check for glib 2.0
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS})

# check for LS2
pkg_check_modules(LS2 REQUIRED luna-service2++)
include_directories(${LS2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LS2_CFLAGS_OTHER})

# check for pbnjson cpp library
pkg_check_modules(PBNJSON_CPP REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CPP_CFLAGS})

# check for pmloglib library
pkg_check_modules(PMLOG REQUIRED PmLogLib>=3)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS})

link_directories(${GLIB2_LIBRARY_DIRS}
                 ${LS1_LIBRARY_DIRS}
                 ${PBNJSON_CPP_LIBRARY_DIRS}
                 ${PMLOG_LIBRARY_DIRS})

add_subdirectory(src)
