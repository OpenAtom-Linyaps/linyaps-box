pkg_check_modules(SECCOMP REQUIRED libseccomp)

set(LINK_LIBS
    seccomp
    stdc++)

set(LL_BOX_SOURCES
    # cmake-format: sortable
    main.cpp
    util/common.cpp
    util/filesystem.cpp
    util/semaphore.cpp
    util/debug/debug.cpp
    util/platform.cpp
    util/logger.cpp
    util/message_reader.cpp
    container/container.cpp
    container/mount/host_mount.cpp
    container/mount/filesystem_driver.cpp
    container/seccomp.cpp
    )
add_executable(ll-box ${LL_BOX_SOURCES})
target_link_libraries(ll-box ${LINK_LIBS})

if (BUILD_STATIC)
  add_executable(ll-box-static ${LL_BOX_SOURCES})
  target_link_libraries(ll-box-static -static ${LINK_LIBS})
  install(TARGETS ll-box-static RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)
endif (BUILD_STATIC)

install(TARGETS ll-box RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
#install(TARGETS ll-box RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID)
