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

# Cloud: every variant on plucked notes, wet only, plus a long-feedback case.
foreach(variant 0 1 2 3)
    add_test(NAME render_cloud_variant${variant}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/cloud_v${variant}.wav --seconds 8 --tail 3
            --set family=1 --set variant=${variant} --set swarm=0.7 --set mix=1)
endforeach()
add_test(NAME render_cloud_feedback
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/cloud_feedback.wav --seconds 8 --tail 6
        --set family=1 --set tail=1 --set scatter=1 --set swarm=0.7 --set mix=0.7)

# Tap: every variant on plucked notes at 1/8 (120 BPM fallback), wet only, plus a long-feedback case.
foreach(variant 0 1 2 3)
    add_test(NAME render_tap_variant${variant}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/tap_v${variant}.wav --seconds 8 --tail 3
            --gain -6 --set family=3 --set variant=${variant} --set swarm=0.64 --set span=0.25 --set mix=1)
endforeach()
add_test(NAME render_tap_feedback
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/tap_feedback.wav --seconds 8 --tail 6
        --set family=3 --set tail=0.8 --set scatter=0.6 --set swarm=0.64 --set mix=0.6)

# Shard: every variant on plucked notes at 1/16 (120 BPM fallback), wet only, plus a feedback case.
foreach(variant 0 1 2 3)
    add_test(NAME render_shard_variant${variant}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/shard_v${variant}.wav --seconds 8 --tail 3
            --gain -6 --set family=2 --set variant=${variant} --set swarm=0.5 --set span=0.5 --set scatter=0.4 --set mix=1)
endforeach()
add_test(NAME render_shard_feedback
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/shard_feedback.wav --seconds 8 --tail 6
        --set family=2 --set tail=0.8 --set scatter=0.6 --set swarm=0.5 --set mix=0.6)

# Hold: freeze at 2 s, release at 5 s (loop = one beat at the 120 BPM fallback), alone and through two engines.
add_test(NAME render_hold_plain
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/hold_plain.wav --seconds 8 --gain -6
        --set mix=0 --at 2.0 hold=1 --at 5.0 hold=0)
add_test(NAME render_hold_cloud
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/hold_cloud.wav --seconds 8 --tail 3 --gain -6
        --set family=1 --set swarm=0.7 --set mix=0.7 --at 2.0 hold=1 --at 5.0 hold=0)
add_test(NAME render_hold_tap
    COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/hold_tap.wav --seconds 8 --tail 3 --gain -6
        --set family=3 --set swarm=0.64 --set mix=0.6 --at 2.0 hold=1 --at 5.0 hold=0)

# Looper (120 BPM fallback, notes loop every 0.5 s... record 1 s, play, overdub, reverse, burst, undo), Post-FX and Pre-FX.
foreach(position 0 1)
    add_test(NAME render_looper_pos${position}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/looper_pos${position}.wav --seconds 12 --tail 2 --gain -9
            --set family=3 --set swarm=0.64 --set mix=0.5 --set looppos=${position}
            --at 0.0 loop=1 --at 2.0 loop=2 --at 4.0 loop=3 --at 6.0 loop=2
            --at 7.0 loopreverse=1 --at 8.0 loopreverse=0 --at 8.5 loopburst=1 --at 10.0 loopburst=0 --at 10.5 loopundo=1)
endforeach()

# Factory presets: each one on plucked notes (WAVs: preset_00.wav ... preset_15.wav, for listening).
foreach(n RANGE 0 15)
    string(LENGTH "${n}" len)
    if(len EQUAL 1)
        set(nn "0${n}")
    else()
        set(nn "${n}")
    endif()
    add_test(NAME render_preset_${nn}
        COMMAND grain_render --gen notes ${CMAKE_CURRENT_BINARY_DIR}/preset_${nn}.wav --seconds 8 --tail 4 --gain -6 --program ${n})
endforeach()

# Keeps the harness itself from rotting: generated tone must pass every check and come out unchanged.
add_test(NAME render_smoke
    COMMAND grain_render --gen tone440 ${CMAKE_CURRENT_BINARY_DIR}/render_smoke.wav --set mix=0 --expect-identity)
