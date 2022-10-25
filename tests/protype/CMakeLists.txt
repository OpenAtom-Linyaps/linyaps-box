set(LINK_LIBS
    stdc++)

add_executable(ll-wrapper
               main.cpp
               ../../src/util/semaphore.cpp
               ../../src/util/filesystem.cpp
               ../../src/util/common.cpp)

target_link_libraries(ll-wrapper ${LINK_LIBS})

install(TARGETS ll-wrapper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
