configure_file(create-star-runtime-dir.in "${CMAKE_CURRENT_BINARY_DIR}/create-star-runtime-dir" @ONLY)
configure_file(cleanup-planet-satellite-runtime-dir.in "${CMAKE_CURRENT_BINARY_DIR}/cleanup-planet-satellite-runtime-dir" @ONLY)

# Oneshot files
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/create-star-runtime-dir
                 ${CMAKE_CURRENT_BINARY_DIR}/cleanup-planet-satellite-runtime-dir
                 mount-squash-package umount-squash-package
                 correct-users
        DESTINATION ${INSTALL_BIN_DIR}
        COMPONENT data)
