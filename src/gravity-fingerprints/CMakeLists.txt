set(Fingerprints_SRCS
    main.cpp
    appliancecryptoservice.cpp
    fingerprintsservice.cpp
    filesystemcertificatestoreprovider.cpp
    genericfingerprintprovider.cpp
)

qt5_add_dbus_adaptor(Fingerprints_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.Fingerprints.xml
                     fingerprintsservice.h FingerprintsService)
qt5_add_dbus_adaptor(Fingerprints_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.ApplianceCrypto.xml
                     appliancecryptoservice.h ApplianceCryptoService)

# final lib
add_executable(gravity-fingerprints ${Fingerprints_SRCS})

target_link_libraries(gravity-fingerprints
                      Fingerprints
                      Qt5::Core
                      Qt5::DBus
                      ${LIBSYSTEMD_DAEMON_LIBRARIES}
                      ${LIBSYSTEMD_JOURNAL_LIBRARIES})

configure_file(dbus-com.ispirata.Hemera.Fingerprints.service.in "${CMAKE_CURRENT_BINARY_DIR}/dbus-com.ispirata.Hemera.Fingerprints.service" @ONLY)

# Install phase
install(TARGETS gravity-fingerprints
        EXPORT  FingerprintsTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT gravity-fingerprints)

install(FILES com.ispirata.Hemera.Fingerprints.service
        DESTINATION ${HEMERA_GRAVITY_DBUS_SYSTEM_ACTIVATION_DIR}
        COMPONENT gravity-fingerprints)

install(FILES com.ispirata.Hemera.Fingerprints.conf
        DESTINATION ${HA_DBUS_SYSTEM_POLICY_DIR}
        COMPONENT gravity-fingerprints)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dbus-com.ispirata.Hemera.Fingerprints.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT gravity-fingerprints)
