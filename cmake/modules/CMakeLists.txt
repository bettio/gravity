configure_file(GravityPaths.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/GravityPaths.cmake" @ONLY)

set(gravity_CMAKEMODULES
    ${CMAKE_CURRENT_BINARY_DIR}/GravityPaths.cmake
)

# CMake modules
install(FILES ${gravity_CMAKEMODULES}
        DESTINATION ${INSTALL_CMAKE_DIR}/Gravity
        COMPONENT cmake)
