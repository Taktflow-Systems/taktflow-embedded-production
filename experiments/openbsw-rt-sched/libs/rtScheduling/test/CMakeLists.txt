# Copyright 2026 Taktflow.

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(rtSchedulingTest
    src/FeasibilityTest.cpp
    src/RtaTest.cpp
    src/RegistryTest.cpp
    src/MonitoredRunnableTest.cpp
    src/JsonTest.cpp)

target_link_libraries(rtSchedulingTest PRIVATE rtScheduling GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(rtSchedulingTest)
