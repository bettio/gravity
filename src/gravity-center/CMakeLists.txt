# Add static plugins
add_subdirectory(plugins)

set(GravityCenter_SRCS
    main.cpp
    core.cpp
)

# final lib
add_executable(gravity-center ${GravityCenter_SRCS})

qt5_use_modules(gravity-center Core Network DBus)
hemera_qt5_sdk_use_modules(gravity-center Core)

target_link_libraries(gravity-center
                      Supermassive
                      Qt5::Core Qt5::Network Qt5::DBus
                      HemeraQt5SDK::Core
                      ${LIBSYSTEMD_DAEMON_LIBRARIES})# gravity-static-plugins) - NEXT
if (NOT HEMERA_GRAVITY_DEVELOPMENT_RELEASE)
set(GRAVITY_RESTART_INSTRUCTIONS
"Restart=always
WatchdogSec=30s

StartLimitInterval=1min
StartLimitBurst=2
StartLimitAction=reboot")
endif ()

configure_file(gravity-center.service.in "${CMAKE_CURRENT_BINARY_DIR}/gravity-center.service" @ONLY)

# Install phase
install(TARGETS gravity-center
        EXPORT  HemeraGravityCenterTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT gravity)

install(FILES com.ispirata.Hemera.GravityCenter.conf
        DESTINATION ${HA_DBUS_SYSTEM_POLICY_DIR}
        COMPONENT gravity)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gravity-center.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT gravity)
