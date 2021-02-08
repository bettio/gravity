# Useful macros
macro(gravity_internal_generate_headers_prefix _prefix _module _output _list)
    # This loop generates the headers
    foreach (f ${${_list}})
        string(TOLOWER ${f} gtmp)
        set(gen-header ${_prefix}${gtmp}.h)

        if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${gen-header}")
            message(FATAL_ERROR "The file ${f} does not have a corresponding ${gen-header} file. Aborting.")
        endif ()

        file(WRITE ${CMAKE_BINARY_DIR}/include/Gravity${_module}/${f} "#include \"${gen-header}\"\n")
        # Copy the file as well. needed in the next target
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${gen-header} DESTINATION ${CMAKE_BINARY_DIR}/include/Gravity${_module}/)

        list(APPEND ${_output} ${CMAKE_BINARY_DIR}/include/Gravity${_module}/${f} ${CMAKE_CURRENT_SOURCE_DIR}/${gen-header})
    endforeach (f)
endmacro()

add_subdirectory(RemountHelper)
add_subdirectory(Fingerprints)
add_subdirectory(Supermassive)
