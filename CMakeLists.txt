cmake_minimum_required(VERSION 3.15)
message("Unnamed O2 Game Clone Project - CMake Build System")

# check if CXX_COMPILER is not cl.exe then set 

project(O2Game)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(O2GAME_PLATFORM x64)
else()
	set(O2GAME_PLATFORM x86)
endif()

message("Platform: ${O2GAME_PLATFORM} - ${CMAKE_SIZEOF_VOID_P}")

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_compile_options(/utf-8 /D_CRT_SECURE_NO_WARNINGS)
else()
    set(CMAKE_C_FLAGS "-m64")
    set(CMAKE_CXX_FLAGS "-m64")
    add_definitions(-D__cpp_lib_char8_t)
    add_definitions(-D__cpp_lib_clamp)
    add_compile_options(-finput-charset=UTF-8)
    add_compile_options(-Wall -Wextra -pedantic)
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    if (MSVC)
        set(CMAKE_C_FLAGS_DEBUG "/Od /ZI /DVK_NO_PROTOTYPES")
        set(CMAKE_CXX_FLAGS_DEBUG "/Od /ZI /DVK_NO_PROTOTYPES")
    else()
        set(CMAKE_C_FLAGS_DEBUG "-g -DVK_NO_PROTOTYPES")
        set(CMAKE_CXX_FLAGS_DEBUG "-g -DVK_NO_PROTOTYPES")
    endif()
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    if (MSVC)
        set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /Oi /Ot /GL /DVK_NO_PROTOTYPES")
        set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /Oi /Ot /GL /DVK_NO_PROTOTYPES")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
    else()
        set(CMAKE_C_FLAGS_RELEASE "-O3 -DVK_NO_PROTOTYPES")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DVK_NO_PROTOTYPES")
    endif()
endif()

set(GFLAGS_USE_TARGET_NAMESPACE ON)

# BEGIN PACKAGES
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
set(SDL2_IMAGE_LIBRARIES $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
set(SQLite3_LIBRARIES unofficial::sqlite3::sqlite3)

find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(BZip2 REQUIRED)
find_package(Iconv REQUIRED)
find_package(sol2 CONFIG REQUIRED)
set(Sol2_LIBRARIES sol2::sol2)

find_package(Lua REQUIRED)

find_package(Vulkan)

if (NOT DEFINED Vulkan_INCLUDE_DIRS)
    if (NOT DEFINED ENV{VULKAN_SDK})
		message(FATAL_ERROR "Cannot find Vulkan and VULKAN_SDK environment variable not defined")
	endif()

	set(Vulkan_INCLUDE_DIRS $ENV{VULKAN_SDK}/include)
	set(Vulkan_LIBRARIES $ENV{VULKAN_SDK}/lib/vulkan-1.lib)
endif()
# END PACKAGES

include_directories(
	third-party/headers 
	${Vulkan_INCLUDE_DIRS} 
	${SDL2_INCLUDE_DIRS}
	${GLM_INCLUDE_DIRS} 
    ${Iconv_INCLUDE_DIRS} 
)

if (WIN32)
	set(THIRD_PARTY_LIBS
		"${PROJECT_SOURCE_DIR}/third-party/lib/${O2GAME_PLATFORM}-windows/$<CONFIG>/bass.lib" 
		"${PROJECT_SOURCE_DIR}/third-party/lib/${O2GAME_PLATFORM}-windows/$<CONFIG>/bass_fx.lib" 
	)

elseif (UNIX AND NOT APPLE)
    set(THIRD_PARTY_LIBS 
        "${PROJECT_SOURCE_DIR}/third-party/lib/${O2GAME_PLATFORM}-linux/$<CONFIG>/libbass.so"
        "${PROJECT_SOURCE_DIR}/third-party/lib/${O2GAME_PLATFORM}-linux/$<CONFIG>/libbass_fx.so"
        dl 
        pthread 
        m 
    )
endif()

set(LIB_FREETYPE Freetype::Freetype)
set(LIB_JPEG JPEG::JPEG)
set(LIB_PNG PNG::PNG)
set(LIB_BZIP2 BZip2::BZip2)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XCB_KHR)
endif()

set(O2GAME_LIBRARIES
    ${SDL2_LIBRARIES} 
    ${SDL2_IMAGE_LIBRARIES} 
    ${SQLite3_LIBRARIES} 
    ${Sol2_LIBRARIES} 
    ${LUA_LIBRARIES}
    ${LIB_FREETYPE} 
    ${LIB_PNG} 
    ${LIB_JPEG} 
    ${LIB_BZIP2} 
    ${THIRD_PARTY_LIBS} 
    ${Iconv_LIBRARIES}
)

message("Libraries: ${O2GAME_LIBRARIES}")

add_subdirectory(Engine)
add_subdirectory(Game)