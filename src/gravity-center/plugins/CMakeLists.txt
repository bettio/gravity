# Macro for config file
MACRO(MACRO_BOOL_TO_01 FOUND_VAR )
   FOREACH (_current_VAR ${ARGN})
      IF(${FOUND_VAR})
         SET(${_current_VAR} 1)
      ELSE(${FOUND_VAR})
         SET(${_current_VAR} 0)
      ENDIF(${FOUND_VAR})
   ENDFOREACH(_current_VAR)
ENDMACRO(MACRO_BOOL_TO_01)

set(gravitystaticplugins_SRCS  )

configure_file(config-static-plugins.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-static-plugins.h )

# final lib
add_library(gravity-static-plugins STATIC ${gravitystaticplugins_SRCS})

qt5_use_modules(gravity-static-plugins Core Network DBus)
