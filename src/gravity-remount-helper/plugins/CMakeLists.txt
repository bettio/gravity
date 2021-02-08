set(TmpfilesHook_SRCS
    tmpfileshook.cpp
)

# final lib
add_library(TmpfilesHook SHARED ${TmpfilesHook_SRCS})

target_link_libraries(TmpfilesHook Qt5::Core HemeraQt5SDK::Core RemountHelper)

# Install phase
install(TARGETS TmpfilesHook
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_REMOUNT_HOOKS_DIR}" COMPONENT shlib
        ARCHIVE DESTINATION "${INSTALL_REMOUNT_HOOKS_DIR}" COMPONENT shlib
        COMPONENT GravityPlugins)

