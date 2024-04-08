cmake_minimum_required(VERSION 3.8.0)

# cmake current directory + headers
SET(EST_AUDIO_HEADERS ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")

if (WIN32)
    message("[EstAudio] Windows platform detected")
    SET(EST_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/platform/win32/EstAudio.lib CACHE INTERNAL "")
    SET(EST_AUDIO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/platform/win32/EstAudio.dll CACHE INTERNAL "")
elseif (APPLE)
    message("[EstAudio] Apple platform detected")
    message(FATAL_ERROR "[EstAudio] Apple platform not supported yet")

    SET(EST_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/platform/mac/EstAudio.dylib CACHE INTERNAL "")
    SET(EST_AUDIO_FILES CACHE INTERNAL "")
elseif (UNIX)
    message("[EstAudio] Linux platform detected")
    message(WARNING "[EstAudio] TODO: add linux blob file to cmake")
    message(FATAL_ERROR "[EstAudio] Linux platform not supported yet")

    SET(EST_AUDIO_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/EstAudio.so CACHE INTERNAL "")
    SET(EST_AUDIO_FILES CACHE INTERNAL "")
endif()
