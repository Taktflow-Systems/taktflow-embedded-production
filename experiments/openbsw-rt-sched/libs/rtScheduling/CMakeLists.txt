# Copyright 2026 Taktflow.
#
# rtScheduling — declarative real-time scheduling layer for openbsw.
#
# Two targets:
#   rtScheduling        — the pure library (no async dependency). Always built.
#   rtSchedulingAsync   — INTERFACE lib that pulls in openbsw's async bridge.
#                         Only declared when the `async` target exists in the
#                         current build (i.e. openbsw is in the tree).
#
# This split means the pure library can be unit-tested without openbsw, and
# the async bridge only gets compiled when we're building on top of openbsw.

add_library(rtScheduling STATIC
    src/Registry.cpp
    src/Json.cpp)

target_include_directories(rtScheduling PUBLIC include)

# C++17 is what openbsw uses; keep parity.
target_compile_features(rtScheduling PUBLIC cxx_std_17)

# Optional async bridge. Only enabled if openbsw's `async` target is present.
if (TARGET async)
    add_library(rtSchedulingAsync INTERFACE)
    target_link_libraries(rtSchedulingAsync INTERFACE rtScheduling async)
    target_include_directories(rtSchedulingAsync INTERFACE include)
endif ()

if (RT_SCHED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif ()
