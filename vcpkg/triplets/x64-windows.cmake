set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# set CXX flags to address sanitizer
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")

if (PORT MATCHES "fmt") 
    set(VCPKG_LIBRARY_LINKAGE static)
endif()