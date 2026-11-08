cmake_minimum_required(VERSION 3.24)

project(IsoCamLock VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Build CommonLibSSE-NG (alandtse fork, supports 1.5.97 / 1.6.x / 1.7.x) from the submodule.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/CommonLibSSE-NG CommonLibSSE EXCLUDE_FROM_ALL)
# The library only loads its plugin helper on the prebuilt path, so load it here for source builds.
include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

# add_commonlibsse_plugin comes from CommonLibSSE-NG's CMake. It creates the DLL target
# and generates the SKSE plugin version data (name, author, address-library flag).
add_commonlibsse_plugin(${PROJECT_NAME}
	AUTHOR "Nick"
	SOURCES src/main.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

# CommonLib's headers expect its precompiled header to be loaded first in every source file,
# including the version-info file the helper above generates.
target_precompile_headers(${PROJECT_NAME} PRIVATE <SKSE/SKSE.h> <RE/Skyrim.h>)

# Copy the built DLL into a ready-to-zip mod folder after each build.
set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/IsoCamLock.ini" "${DIST_DIR}"
)
