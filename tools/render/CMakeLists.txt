juce_add_console_app(grain_render PRODUCT_NAME "grain_render")

target_sources(grain_render PRIVATE main.cpp ${GRAIN_CORE_SOURCES})
target_include_directories(grain_render PRIVATE ${GRAIN_INCLUDE_DIR})
target_compile_definitions(grain_render PRIVATE ${GRAIN_JUCE_DEFS})
target_link_libraries(grain_render PRIVATE ${GRAIN_JUCE_LIBS} juce::juce_audio_formats)

foreach(mode 0 1 2 3)
    add_test(NAME render_haze_mode${mode}
        COMMAND grain_render --gen impulse ${CMAKE_CURRENT_BINARY_DIR}/haze${mode}.wav --set haze=0.5 --set hazemode=${mode} --set mix=0 --tail 10)
endforeach()
add_test(NAME render_dusk
    COMMAND grain_render --gen sweep ${CMAKE_CURRENT_BINARY_DIR}/dusk.wav --set dusk=800 --set mix=0)
add_test(NAME render_bend
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/bend.wav --set bend=1 --set mix=0)

# Auto Gain: the leveler is transparent (identity) on quiet and normal sources; the ceiling holds on hot ones.
add_test(NAME render_autogain_quiet
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/ag_quiet.wav --gain -30 --seconds 10 --set autogain=1 --set mix=0 --expect-identity)
add_test(NAME render_autogain_normal
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/ag_normal.wav --seconds 6 --set autogain=1 --set mix=0 --expect-identity)
add_test(NAME render_ceiling
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/ceiling.wav --gain 12 --set autogain=1 --set mix=0)

# Mosaic: every variant on plucked notes, wet only; WAVs land in the build dir for listening.
foreach(variant 0 1 2 3)
    add_test(NAME render_mosaic_variant${variant}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/mosaic_v${variant}.wav --seconds 8 --tail 3
            --set family=0 --set variant=${variant} --set swarm=0.8 --set mix=1)
endforeach()
add_test(NAME render_mosaic_busy_scatter
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/mosaic_busy.wav --seconds 8 --tail 3
        --set swarm=1 --set scatter=1 --set tail=1 --set span=0.2 --set mix=0.6)

# Keeps the harness itself from rotting: generated tone must pass every check and come out unchanged.
add_test(NAME render_smoke
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/render_smoke.wav --set mix=0 --expect-identity)
