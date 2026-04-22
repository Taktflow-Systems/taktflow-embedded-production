# Copyright 2026 Taktflow.
#
# Demo target. Builds only when openbsw's `async` library is visible in the
# build — otherwise the demo depends on symbols that don't exist.
#
# The demo is a STATIC library, not an executable. It contains the pieces a
# real openbsw application would drop in: three IRunnable-shaped systems,
# a single manifest.cpp with the declarative schedule, and a bootstrap()
# function to call from the host's app::startApp(). Linking this into the
# referenceApp would demonstrate the end-to-end flow.

if (NOT TARGET async)
    message(STATUS "rtScheduling demo: openbsw 'async' target not present, skipping demo.")
    return()
endif ()

add_library(rtSchedDemo STATIC
    src/SafetySystem.cpp
    src/ControlSystem.cpp
    src/TelemetrySystem.cpp
    src/manifest.cpp)

target_include_directories(rtSchedDemo PUBLIC include)
target_link_libraries(rtSchedDemo PUBLIC rtSchedulingAsync async)
target_compile_features(rtSchedDemo PUBLIC cxx_std_17)
