project(gravity CXX)

cmake_minimum_required(VERSION 2.8.9)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
find_package(CommonModules REQUIRED)

#################################################################################################
# Modify these variables to what suits the application best. Remember, no dashes in the version!
set(HEMERA_GRAVITY_MAJOR_VERSION 0)
set(HEMERA_GRAVITY_MINOR_VERSION 91)
set(HEMERA_GRAVITY_RELEASE_VERSION 106)
option(HEMERA_GRAVITY_DEVELOPMENT_RELEASE "Must be ON unless we're releasing" ON)

option(ENABLE_WERROR "Enables WError. Always enable when developing, and disable when releasing." ON)

set(HEMERA_GRAVITY_GRAVITY_DIR /etc/hemera/gravity CACHE PATH "Location of the directory for the gravity files.")
set(HEMERA_GRAVITY_ORBIT_DIR /etc/hemera/gravity/orbit.d CACHE PATH "Location of the directory for the orbit files.")
set(HEMERA_GRAVITY_ENVIRONMENT_DIR ${CMAKE_INSTALL_PREFIX}/share/hemera CACHE PATH "Location of the environment files for orbits and displays.")
set(HEMERA_GRAVITY_USERLISTS_DIR ${CMAKE_INSTALL_PREFIX}/share/hemera/userlists CACHE PATH "Location of the userslist files from gravity-compiler.")
set(HEMERA_GRAVITY_CONFIGS_DIR ${CMAKE_INSTALL_PREFIX}/share/hemera/gravity-configs CACHE PATH "Location of the installed Gravity configuration files.")
set(HEMERA_GRAVITY_DBUS_SYSTEM_ACTIVATION_DIR ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services CACHE PATH "Location of DBus activatable system services.")
set(HEMERA_GRAVITY_SYSTEMD_USER_UNITS_DIR ${CMAKE_INSTALL_PREFIX}/lib/systemd/user CACHE PATH "Location of Systemd user units.")
set(HEMERA_GRAVITY_USER_HOME_DIR /var/lib/hemera CACHE PATH "Location of Hemera users home directories.")

#################################################################################################

set(HEMERA_GRAVITY_VERSION ${HEMERA_GRAVITY_MAJOR_VERSION}.${HEMERA_GRAVITY_MINOR_VERSION}.${HEMERA_GRAVITY_RELEASE_VERSION})
if (HEMERA_GRAVITY_DEVELOPMENT_RELEASE)
    include(GetGitRevisionDescription)
    get_git_head_revision(GIT_REFSPEC GIT_SHA1)
    STRING(SUBSTRING ${GIT_SHA1} 0 8 GIT_SHA1_SMALL)
    set(HEMERA_GRAVITY_VERSION_STRING ${HEMERA_GRAVITY_MAJOR_VERSION}.${HEMERA_GRAVITY_MINOR_VERSION}.${HEMERA_GRAVITY_RELEASE_VERSION}+git${GIT_SHA1_SMALL})
else (HEMERA_GRAVITY_DEVELOPMENT_RELEASE)
    set(HEMERA_GRAVITY_VERSION_STRING ${HEMERA_GRAVITY_MAJOR_VERSION}.${HEMERA_GRAVITY_MINOR_VERSION}.${HEMERA_GRAVITY_RELEASE_VERSION})
endif (HEMERA_GRAVITY_DEVELOPMENT_RELEASE)

find_package(Qt5 COMPONENTS Core Concurrent Network DBus Qml REQUIRED)
find_package(HemeraQt5SDK 0.8.90 COMPONENTS Core Generators Qml REQUIRED)
find_package(Systemd REQUIRED)
# hemera-fingerprints requires this to generate hardware id
find_package(UDev REQUIRED)
# usb-gadget-manager and other components need to interact with connman
find_package(LibConnmanQt5 REQUIRED)
# Needed for finding modules with pkg-config
find_package(PkgConfig REQUIRED)

# Check if Qt SDK private headers have been installed
unset(QT_SDK_PVTHDRS CACHE)
find_path(QT_SDK_PVTHDRS private/HemeraCore/hemeraasyncinitobject_p.h HINTS ${HemeraQt5SDKCore_INCLUDE_DIRS})
if (QT_SDK_PVTHDRS STREQUAL QT_SDK_PVTHDRS-NOTFOUND)
    message(FATAL_ERROR "Hemera Qt5 SDK's private headers have not been found.\nYou need to compile Hemera Qt5 SDK with -DHEMERA_QT5_SDK_PRIVATE_COMPONENTS=ON to build Gravity.")
endif ()

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${SYSTEMD_INCLUDE_DIR} ${UDEV_INCLUDE_DIR} ${LIBCONNMANQT5_INCLUDE_DIR})
include_directories(${CMAKE_BINARY_DIR}/include)
add_definitions(-DGRAVITY_VERSION="${HEMERA_GRAVITY_VERSION_STRING}")

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include(EnableCoverageReport)
include(EnableSlocCount)
include(GenerateCppcheck)
include(GitArchive)
include(CompilerWarnings)
include(TestCXXAcceptsFlag)

# Warnings. Enable WError, this component is critical.
if(${ENABLE_WERROR})
    set(RET_W 1)
else(${ENABLE_WERROR})
    set(RET_W 0)
endif(${ENABLE_WERROR})

set(desired
    all
    extra
    sign-compare
    pointer-arith
    format-security
    init-self
    non-virtual-dtor)
set(undesired
    missing-field-initializers
    unused-parameter)
compiler_warnings(CMAKE_CXX_FLAGS_WARNINGS cxx ${RET_W} "${desired}" "${undesired}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_WARNINGS}")

# C++11
if (CMAKE_COMPILER_IS_GNUCXX)
   execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
   if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
        message(STATUS "C++11 activated.")
        add_definitions("-std=gnu++11")
   elseif (GCC_VERSION VERSION_GREATER 4.3 OR GCC_VERSION VERSION_EQUAL 4.3)
        message(WARNING "C++0x activated. If you get any errors update to a compiler which fully supports C++11")
        add_definitions("-std=gnu++0x")
   else ()
        message(FATAL_ERROR "C++11 needed. Therefore a gcc compiler with a version higher than 4.3 is needed.")
   endif()
else (CMAKE_COMPILER_IS_GNUCXX)
   add_definitions("-std=c++0x")
endif (CMAKE_COMPILER_IS_GNUCXX)

# When building Gravity, we want every symbol to be resolvable immediately even if we're building a shared library.
add_definitions(-Wl,-z,defs)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z defs")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -z defs")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z defs")

# We want explicit literals all over the place, and fast concat
add_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_USE_QSTRINGBUILDER)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR libexec/hemera CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set(INSTALL_PLUGINS_DIR lib/hemera/gravity-center/plugins CACHE PATH "Installation directory for plugins")
set(INSTALL_REMOUNT_HOOKS_DIR lib/hemera/gravity-remount-helper/hooks CACHE PATH "Installation directory for remount helper hooks")
set(INSTALL_FINGERPRINTS_PLUGINS_DIR lib/hemera/fingerprints/plugins CACHE PATH "Installation directory for hemera-fingerprints plugins")
set(INSTALL_APPLIANCECRYPTO_PLUGINS_DIR lib/hemera/appliance-crypto/plugins CACHE PATH "Installation directory for hemera-appliancecrypto plugins")
set(INSTALL_PKGCONFIG_DIR ${INSTALL_LIB_DIR}/pkgconfig CACHE PATH "Installation directory for pkgconfig files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake)
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# We need the fully qualified paths
foreach(p BIN CMAKE INCLUDE PLUGINS REMOUNT_HOOKS FINGERPRINTS_PLUGINS APPLIANCECRYPTO_PLUGINS)
  set(var INSTALL_${p}_DIR)
  set(destvar FULL_INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${destvar} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  else()
    set(${destvar} "${${var}}")
  endif()
endforeach()

# Enable compilation of tests and examples by default only if we're not in a submodule
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(ENABLE_GRAVITY_EXAMPLES "Enable compilation of examples" ON)
    option(ENABLE_GRAVITY_TESTS "Enable compilation of tests" ON)
else (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(ENABLE_GRAVITY_EXAMPLES "Enable compilation of examples" OFF)
    option(ENABLE_GRAVITY_TESTS "Enable compilation of tests" OFF)
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_GRAVITY_COVERAGE "Enable compiler coverage" OFF)

# Create paths for the GravityConfig.cmake and GravityConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${FULL_INSTALL_CMAKE_DIR}/Gravity" "${FULL_INSTALL_INCLUDE_DIR}")

# Definitions
add_definitions(-DHEMERA_GRAVITY_VERSION="${HEMERA_GRAVITY_VERSION_STRING}")

# Config file
configure_file(gravityconfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/gravityconfig.h" @ONLY)

if (ENABLE_GRAVITY_TESTS)
    hemera_setup_test_targets("Gravity" ${ENABLE_GRAVITY_COVERAGE} 60)
    #add_subdirectory(tests)
endif (ENABLE_GRAVITY_TESTS)

set(PROJECT_VERSION_FULL ${HEMERA_GRAVITY_VERSION_STRING})

configure_file(GravityConfig.cmake.in
  "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/GravityConfig.cmake" @ONLY)
configure_file(${COMMONMODULES_CMAKE_DIR}/BasicFindPackageVersion.cmake.in
  "${CMAKE_BINARY_DIR}/GravityConfigVersion.cmake" @ONLY)

# Install the GravityConfig.cmake and GravityConfigVersion.cmake
install(FILES
  "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/GravityConfig.cmake"
  "${CMAKE_BINARY_DIR}/GravityConfigVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}/Gravity" COMPONENT Gravity)

# sources
add_subdirectory(cmake/modules)
add_subdirectory(lib)
add_subdirectory(share)
add_subdirectory(src)

if (ENABLE_GRAVITY_EXAMPLES)
#     add_subdirectory(testApp)
endif (ENABLE_GRAVITY_EXAMPLES)

# Add these targets only if we are in the root dir
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    # --- sloccount ---

    enable_sloccount(FOLDERS src testApp)

    # --- cppcheck ---

    generate_cppcheck(SOURCES src testApp
                      ENABLE_IDS all
                      INLINE_SUPPRESSION)
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})

# Archive
add_make_dist_target(gravity ${HEMERA_GRAVITY_VERSION_STRING})

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
