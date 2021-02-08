# Require libuser
pkg_check_modules(LIBUSER REQUIRED libuser)

# libuser uses deprecated GValueArray
add_definitions(-Wno-error=deprecated-declarations)

include_directories(${LIBUSER_INCLUDE_DIRS})

set(gravity-user-manager_SRCS
    gravityusermanager.cpp

    main.cpp
)

# final libraries
add_executable(gravity-user-manager ${gravity-user-manager_SRCS})

target_link_libraries(gravity-user-manager Qt5::Core HemeraQt5SDK::Core ${LIBUSER_LIBRARIES} ${LIBSYSTEMD_DAEMON_LIBRARIES})

# Install GRAVITY Compiler
install(TARGETS gravity-user-manager
        RUNTIME DESTINATION "${HA_TOOLS_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${HA_TOOLS_DIR}" COMPONENT shlib
        COMPONENT Tools)
