# puts the DLLs a program needs next to it, so it runs without the toolchain on the PATH
# cmake -DEXECUTABLE=<exe> -DSEARCH_DIR=<folder with the DLLs> -DOBJDUMP=<objdump> -P CopyRuntimeDlls.cmake

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")

# follows dependencies of dependencies: SDL2_ttf.dll alone pulls in freetype, harfbuzz and more
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    DIRECTORIES "${SEARCH_DIR}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    CONFLICTING_DEPENDENCIES_PREFIX conflict
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES "[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
)

get_filename_component(destination "${EXECUTABLE}" DIRECTORY)

# a DLL already copied for an earlier program or build is found twice, next to the program and
# in the toolchain: they're the same file. a name that's also a Windows DLL isn't ours to ship
foreach(name IN LISTS conflict_FILENAMES)
    if(EXISTS "${SEARCH_DIR}/${name}")
        list(APPEND resolved "${SEARCH_DIR}/${name}")
    endif()
endforeach()

foreach(library IN LISTS resolved)
    get_filename_component(folder "${library}" DIRECTORY)

    if(NOT folder STREQUAL destination)
        file(COPY "${library}" DESTINATION "${destination}")
    endif()
endforeach()

foreach(library IN LISTS unresolved)
    message(WARNING "${EXECUTABLE}: could not find ${library}")
endforeach()
