add_executable(grain_tests
    ParametersTest.cpp
    PassthroughTest.cpp
    DspTest.cpp
    AutoGainTest.cpp
    MosaicTest.cpp
    CloudTest.cpp
    TapTest.cpp
    ShardTest.cpp
    HoldTest.cpp
    LooperTest.cpp
    PresetTest.cpp
    MidiTest.cpp
    GainTest.cpp
    RoutingTest.cpp
    AllocGuard.cpp)

target_sources(grain_tests PRIVATE ${GRAIN_CORE_SOURCES})
target_include_directories(grain_tests PRIVATE ${GRAIN_INCLUDE_DIR})
target_compile_definitions(grain_tests PRIVATE ${GRAIN_JUCE_DEFS})
target_link_libraries(grain_tests PRIVATE ${GRAIN_JUCE_LIBS} Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(grain_tests)
