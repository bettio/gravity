macro(supermassive_internal_generate_headers _module _output _list)
    gravity_internal_generate_headers_prefix("gravity" ${_module} ${_output} ${_list})
endmacro()

add_definitions(-DBUILDING_HEMERA_GRAVITY)

set(supermassivelib_SRCS
    gravitygalaxymanager.cpp
    gravityapplication.cpp
    gravityapplicationhandler.cpp
    gravitydbustypes.cpp
    gravitydevicemanagement.cpp
    gravityoperations.cpp
    gravityplugin.cpp
    gravitypluginloader.cpp
    gravityremovablestoragemanager.cpp
    gravitysandbox.cpp
    gravitysandboxmanager.cpp
    gravitysatellitemanager.cpp
    gravitystarsequence.cpp
)

# Register here all the generated uppercase headers. The lowercase ones matching ones in this list will be installed as well
set(supermassivelib_GENHEADERS
    GalaxyManager
    Application
    ApplicationHandler
    DeviceManagement
    GalaxyManager
    Global
    Operations
    Plugin
    PluginLoader
    RemovableStorageManager
    Sandbox
    SandboxManager
    SatelliteManager
    StarSequence
)

supermassive_internal_generate_headers(Supermassive supermassivelib_HEADERS supermassivelib_GENHEADERS)

qt5_add_dbus_adaptor(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/com.ispirata.Hemera.Gravity.StarSequence.xml
                     gravitystarsequence.h Gravity::StarSequence)
qt5_add_dbus_adaptor(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/com.ispirata.Hemera.Gravity.GalaxyManager.xml
                     gravitygalaxymanager.h Gravity::GalaxyManager)
qt5_add_dbus_adaptor(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/com.ispirata.Hemera.Gravity.SatelliteManager.xml
                     gravitysatellitemanager.h Gravity::SatelliteManager)
qt5_add_dbus_adaptor(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.Parsec.ApplicationHandler.xml
                     gravityapplicationhandler.h Gravity::ApplicationHandler)
qt5_add_dbus_adaptor(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.DeviceManagement.xml
                     gravitydevicemanagement.h Gravity::DeviceManagement)
qt5_add_dbus_adaptor(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.RemovableStorageManager.xml
                     gravityremovablestoragemanager.h Gravity::RemovableStorageManager)

qt5_add_dbus_interface(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.Application.xml applicationinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.DBusObject.xml dbusobjectinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/org.freedesktop.systemd1.Manager.xml systemdmanagerinterface)
qt5_add_dbus_interface(supermassivelib_SRCS /usr/share/dbus-1/interfaces/org.freedesktop.locale1.xml localeinterface)
qt5_add_dbus_interface(supermassivelib_SRCS /usr/share/dbus-1/interfaces/org.freedesktop.timedate1.xml timedateinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/org.freedesktop.DBus.xml fdodbusinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/org.freedesktop.DBus.Properties.xml fdodbuspropertiesinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.Planet.xml planetinterface)
qt5_add_dbus_interface(supermassivelib_SRCS ${CMAKE_SOURCE_DIR}/share/dbus/com.ispirata.Hemera.Gravity.SatelliteManager.xml satellitemanagerinterface)

# final lib
add_library(Supermassive SHARED ${supermassivelib_SRCS})

set_target_properties(Supermassive PROPERTIES
                      OUTPUT_NAME GravitySupermassive
                      SOVERSION ${HEMERA_GRAVITY_MAJOR_VERSION}
                      VERSION ${HEMERA_GRAVITY_VERSION}
                      PUBLIC_HEADER "${supermassivelib_HEADERS}")

target_link_libraries(Supermassive
                      Qt5::Core Qt5::Network Qt5::DBus
                      HemeraQt5SDK::Core
                      ${LIBSYSTEMD_DAEMON_LIBRARIES}
                      ${UDEV_LIBS})

# Install phase
install(TARGETS Supermassive
        EXPORT  GravitySupermassiveTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/Gravity/GravitySupermassive"
        COMPONENT GravitySupermassive)

# Export Gravity target
install(EXPORT GravitySupermassiveTargets NAMESPACE Gravity:: DESTINATION ${INSTALL_CMAKE_DIR}/GravitySupermassive)

set(SUPERMASSIVE_INCLUDE_DIRS "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity"
                              "\${CMAKE_CURRENT_LIST_DIR}/${REL_INCLUDE_DIR}/Gravity/GravitySupermassive")
configure_file(GravitySupermassiveConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassiveConfig.cmake @ONLY)
configure_file(${COMMONMODULES_CMAKE_DIR}/BasicFindPackageVersion.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassiveConfigVersion.cmake @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassiveConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassiveConfigVersion.cmake
        DESTINATION ${INSTALL_CMAKE_DIR}/GravitySupermassive COMPONENT Supermassive)

# Pkgconfig
configure_file(GravitySupermassive.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassive.pc @ONLY)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GravitySupermassive.pc
        DESTINATION ${INSTALL_PKGCONFIG_DIR} COMPONENT Supermassive)
