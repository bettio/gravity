set(gravity_MISCFILES
    hemera-star-template.service.in
    hemera-orbit-template.service.in
    hemera-star-template.target.in
    hemera-orbit-template.target.in
    weston-star-template.service.in
    dbus-star-bus/allowance-star-template.conf.in
    dbus-star-bus/star-bus-policy-orbit-template.conf.in
    dbus-star-bus/star-bus-policy-star-template.conf.in
)

# CMake modules
install(FILES ${gravity_MISCFILES}
        DESTINATION ${HA_SDK_DATA_DIR}
        COMPONENT data)

install(DIRECTORY environment
        DESTINATION ${HEMERA_GRAVITY_ENVIRONMENT_DIR})

configure_file(hemera-create-star-runtime-dir@.service.in "${CMAKE_CURRENT_BINARY_DIR}/hemera-create-star-runtime-dir@.service" @ONLY)
install(FILES
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR})

# Install DBus support files
install(FILES dbus-star-bus/hemera-star-bus.conf
        DESTINATION /etc/dbus-1/)
install(FILES dbus-star-bus/dbus-hemera-star@.service
              dbus-star-bus/dbus-hemera-star@.socket
        DESTINATION ${HEMERA_GRAVITY_SYSTEMD_USER_UNITS_DIR})

# Services
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hemera-create-star-runtime-dir@.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT data)

# Scripts
add_subdirectory(scripts)

# Tmpfiles
install(FILES hemera-var.conf hemera-rpm.conf
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/tmpfiles.d/
        COMPONENT data)
