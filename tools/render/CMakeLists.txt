juce_add_console_app(grain_render PRODUCT_NAME "grain_render")

target_sources(grain_render PRIVATE main.cpp ${GRAIN_CORE_SOURCES})
target_include_directories(grain_render PRIVATE ${GRAIN_INCLUDE_DIR})
target_compile_definitions(grain_render PRIVATE ${GRAIN_JUCE_DEFS})
target_link_libraries(grain_render PRIVATE ${GRAIN_JUCE_LIBS} juce::juce_audio_formats)

# Keeps the harness itself from rotting: generated tone must pass every check and come out unchanged.
add_test(NAME render_smoke
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/render_smoke.wav --expect-identity)
