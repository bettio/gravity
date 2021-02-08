# Add plugins
add_subdirectory(plugins)

set(GravityRemountHelper_SRCS
    main.cpp
    gravityremounthelpercore.cpp
)

# final executable
add_executable(gravity-remount-helper ${GravityRemountHelper_SRCS})

target_link_libraries(gravity-remount-helper Qt5::Core HemeraQt5SDK::Core
                      RemountHelper
                      ${LIBSYSTEMD_DAEMON_LIBRARIES})# gravity-remount-helper-static-plugins) - NEXT

configure_file(gravity-remount-helper.service.in "${CMAKE_CURRENT_BINARY_DIR}/gravity-remount-helper.service" @ONLY)

# Install phase
install(TARGETS gravity-remount-helper
        EXPORT  HemeraGravityRemountHelperTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT gravity)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gravity-remount-helper.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT gravity)

