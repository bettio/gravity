set(Parsec_SRCS
    main.cpp
    parseccore.cpp
)

qt5_add_dbus_adaptor(Parsec_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.Parsec.xml
                     parseccore.h ParsecCore)
qt5_add_dbus_interface(Parsec_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/com.ispirata.Hemera.Gravity.StarSequence.xml starsequenceinterface)
qt5_add_dbus_interface(Parsec_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.DBusObject.xml dbusobjectinterface)

# final lib
add_executable(parsec ${Parsec_SRCS})

target_link_libraries(parsec
                      Qt5::Core Qt5::Network Qt5::DBus
                      HemeraQt5SDK::Core
                      Supermassive
                      ${LIBSYSTEMD_DAEMON_LIBRARIES}
                      ${LIBSYSTEMD_LOGIN_LIBRARIES})

configure_file(parsec.service.in "${CMAKE_CURRENT_BINARY_DIR}/parsec.service" @ONLY)

# Install phase
install(TARGETS parsec
        EXPORT  HemeraParsecTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT gravity)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/parsec.service
        DESTINATION ${HA_SYSTEMD_USER_DIR}
        COMPONENT gravity)
