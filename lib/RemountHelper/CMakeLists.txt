include_directories(${CMAKE_SOURCE_DIR}/lib)

# Helper library for plugins
set(gravityremounthelperlib_SRCS
    gravityremounthelperplugin.cpp
)

# Register here all the generated uppercase headers. The lowercase ones matching ones in this list will be installed as well
set(gravityremounthelperlib_GENHEADERS
    Plugin
)

gravity_internal_generate_headers_prefix("gravityremounthelper" RemountHelper gravityremounthelperlib_HEADERS gravityremounthelperlib_GENHEADERS)

# final lib
add_library(RemountHelper SHARED ${gravityremounthelperlib_SRCS})

set_target_properties(RemountHelper PROPERTIES
                      OUTPUT_NAME GravityRemountHelper
                      SOVERSION ${HEMERA_GRAVITY_MAJOR_VERSION}
                      VERSION ${HEMERA_GRAVITY_VERSION}
                      PUBLIC_HEADER "${gravityremounthelperlib_HEADERS}")

target_link_libraries(RemountHelper
                      Qt5::Core
                      HemeraQt5SDK::Core)

# Install phase
install(TARGETS RemountHelper
        EXPORT  GravityRemountHelperTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/Gravity/GravityRemountHelper"
        COMPONENT Gravity)

# Export Gravity target
install(EXPORT GravityRemountHelperTargets NAMESPACE Gravity:: DESTINATION ${INSTALL_CMAKE_DIR}/GravityRemountHelper)

set(REMOUNTHELPER_INCLUDE_DIRS "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity"
                               "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity/GravityRemountHelper")
configure_file(GravityRemountHelperConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelperConfig.cmake @ONLY)
configure_file(${COMMONMODULES_CMAKE_DIR}/BasicFindPackageVersion.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelperConfigVersion.cmake @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelperConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelperConfigVersion.cmake
        DESTINATION ${INSTALL_CMAKE_DIR}/GravityRemountHelper COMPONENT RemountHelper)

# Pkgconfig
configure_file(GravityRemountHelper.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelper.pc @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravityRemountHelper.pc
        DESTINATION ${INSTALL_PKGCONFIG_DIR} COMPONENT RemountHelper)
