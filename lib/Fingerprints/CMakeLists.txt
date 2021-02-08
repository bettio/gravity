# Plugin lib
set(FingerprintsPrivateLib_SRCS
    certificatestoreproviderplugin.cpp
    fingerprintproviderplugin.cpp
)

# Register here all the generated uppercase headers. The lowercase ones matching ones in this list will be installed as well
set(FingerprintsPrivateLib_GENHEADERS
    CertificateStoreProviderPlugin
    FingerprintProviderPlugin
)

gravity_internal_generate_headers_prefix("" Fingerprints FingerprintsPrivateLib_HEADERS FingerprintsPrivateLib_GENHEADERS)

add_library(Fingerprints SHARED ${FingerprintsPrivateLib_SRCS})

set_target_properties(Fingerprints PROPERTIES
                      OUTPUT_NAME GravityFingerprints
                      SOVERSION ${HEMERA_GRAVITY_MAJOR_VERSION}
                      VERSION ${HEMERA_GRAVITY_VERSION}
                      PUBLIC_HEADER "${FingerprintsPrivateLib_HEADERS}")

target_link_libraries(Fingerprints
                      Qt5::Core HemeraQt5SDK::Core
                      ${UDEV_LIBS})

# Install phase
install(TARGETS Fingerprints
        EXPORT  GravityFingerprintsTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/Gravity/GravityFingerprints"
        COMPONENT Gravity)

# Export Gravity target
install(EXPORT GravityFingerprintsTargets NAMESPACE Gravity:: DESTINATION ${INSTALL_CMAKE_DIR}/GravityFingerprints)

set(FINGERPRINTS_INCLUDE_DIRS "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity"
                              "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity/GravityFingerprints")
configure_file(GravityFingerprintsConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprintsConfig.cmake @ONLY)
configure_file(${COMMONMODULES_CMAKE_DIR}/BasicFindPackageVersion.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprintsConfigVersion.cmake @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprintsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprintsConfigVersion.cmake
        DESTINATION ${INSTALL_CMAKE_DIR}/GravityFingerprints COMPONENT Fingerprints)

# Pkgconfig
configure_file(GravityFingerprints.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprints.pc @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravityFingerprints.pc
        DESTINATION ${INSTALL_PKGCONFIG_DIR} COMPONENT Fingerprints)
