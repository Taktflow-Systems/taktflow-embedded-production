# Copyright 2026 Taktflow.
#
# Top-level CMake for the openbsw-rt-sched experiment. Three build shapes:
#
#   Standalone lib + tests (default, no openbsw):
#     cmake -S . -B build -DRT_SCHED_BUILD_TESTS=ON
#     cmake --build build
#     ctest --test-dir build
#
#   With openbsw as a local sibling checkout:
#     cmake -S . -B build \
#           -DRT_SCHED_BUILD_DEMO=ON \
#           -DOPENBSW_SOURCE_DIR=/path/to/openbsw \
#           -DBUILD_TARGET_PLATFORM=POSIX -DBUILD_TARGET_RTOS=FREERTOS
#     cmake --build build
#
#   With openbsw fetched from GitHub:
#     cmake -S . -B build \
#           -DRT_SCHED_BUILD_DEMO=ON \
#           -DRT_SCHED_FETCH_OPENBSW=ON
#     cmake --build build

cmake_minimum_required(VERSION 3.28)
project(openbsw-rt-sched
    VERSION     0.1.0
    DESCRIPTION "Declarative real-time scheduling layer for Eclipse openbsw (issue #370 experiment)"
    LANGUAGES   CXX C)

# ---------------------------------------------------------------------------
# Options

option(RT_SCHED_BUILD_TESTS   "Build rtScheduling unit tests (no openbsw needed)" ON)
option(RT_SCHED_BUILD_DEMO    "Build the demo — requires openbsw in the tree"     OFF)
option(RT_SCHED_FETCH_OPENBSW "FetchContent openbsw from GitHub if not provided"  OFF)
set(OPENBSW_SOURCE_DIR "" CACHE PATH "Path to a local openbsw checkout")

# ---------------------------------------------------------------------------
# openbsw integration (optional)
#
# We only bring openbsw into the build when the demo is requested. The
# core rtScheduling library + tests don't need it.

if (RT_SCHED_BUILD_DEMO)
    if (OPENBSW_SOURCE_DIR)
        message(STATUS "rtScheduling: using local openbsw at ${OPENBSW_SOURCE_DIR}")
        if (NOT BUILD_TARGET_PLATFORM)
            set(BUILD_TARGET_PLATFORM "POSIX" CACHE STRING "" FORCE)
        endif ()
        if (NOT BUILD_TARGET_RTOS)
            set(BUILD_TARGET_RTOS "FREERTOS" CACHE STRING "" FORCE)
        endif ()
        if (NOT BUILD_EXECUTABLE)
            set(BUILD_EXECUTABLE "referenceApp" CACHE STRING "" FORCE)
        endif ()
        add_subdirectory(${OPENBSW_SOURCE_DIR} _openbsw_build EXCLUDE_FROM_ALL)
    elseif (RT_SCHED_FETCH_OPENBSW)
        message(STATUS "rtScheduling: fetching openbsw from GitHub")
        include(FetchContent)
        FetchContent_Declare(openbsw
            GIT_REPOSITORY https://github.com/eclipse-openbsw/openbsw.git
            GIT_TAG        main)
        set(BUILD_TARGET_PLATFORM "POSIX"       CACHE STRING "" FORCE)
        set(BUILD_TARGET_RTOS     "FREERTOS"    CACHE STRING "" FORCE)
        set(BUILD_EXECUTABLE      "referenceApp" CACHE STRING "" FORCE)
        FetchContent_MakeAvailable(openbsw)
    else ()
        message(FATAL_ERROR
            "RT_SCHED_BUILD_DEMO=ON requires either OPENBSW_SOURCE_DIR=<path> "
            "or RT_SCHED_FETCH_OPENBSW=ON")
    endif ()
endif ()

# ---------------------------------------------------------------------------
# rtScheduling library — always built.

add_subdirectory(libs/rtScheduling)

# ---------------------------------------------------------------------------
# Demo — only if openbsw is in the tree and the demo was requested.

if (RT_SCHED_BUILD_DEMO AND TARGET async)
    add_subdirectory(demo)
endif ()
