pkg_check_modules(SECCOMP REQUIRED libseccomp)

set(LINK_LIBS
    seccomp
    stdc++)

add_executable(ll-box
               main.cpp
               loader.cpp
               util/common.cpp
               util/filesystem.cpp
               util/semaphore.cpp
               util/debug/debug.cpp
               util/platform.cpp
               container/container.cpp
               container/seccomp.cpp)

target_link_libraries(ll-box ${LINK_LIBS})

install(TARGETS ll-box RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID)
