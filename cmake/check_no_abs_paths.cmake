# Used as: cmake -DELF=... -DUF2=... -P cmake/check_no_abs_paths.cmake
# PRD §5.1 / T21: the flashed image must not contain absolute source paths.

if(NOT DEFINED ELF OR NOT EXISTS "${ELF}")
    message(FATAL_ERROR "check_no_abs_paths: ELF is missing (${ELF})")
endif()
if(NOT DEFINED UF2 OR NOT EXISTS "${UF2}")
    message(FATAL_ERROR "check_no_abs_paths: UF2 is missing (${UF2})")
endif()

function(assert_no_abs_paths file mode)
    execute_process(
        COMMAND strings ${mode} "${file}"
        OUTPUT_VARIABLE text
        RESULT_VARIABLE rc
    )
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "check_no_abs_paths: strings failed on ${file}")
    endif()
    if(text MATCHES "/home/" OR text MATCHES "/Users/")
        message(FATAL_ERROR "absolute source paths found in ${file}")
    endif()
endfunction()

# Loadable ELF sections (not DWARF). UF2 is the flashed image.
assert_no_abs_paths("${ELF}" -d)
assert_no_abs_paths("${UF2}" -a)
