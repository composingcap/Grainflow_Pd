cmake_minimum_required(VERSION 3.19)
project(GRAINFLOW_PD)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MACOSX_RPATH ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()
if(APPLE)
    set(CMAKE_BUILD_RPATH "@loader_path")
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)


include(${CMAKE_CURRENT_SOURCE_DIR}/pd.cmake)

set_pd_sources("${CMAKE_CURRENT_SOURCE_DIR}/source/pure-data/src")
set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/grainflow/)
set(PD_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/source/pure-data/src/pd.lib)
include_directories(grainflowLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/grainflowLib)

SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source/projects)
foreach (project_dir ${PROJECT_DIRS})
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir}/CMakeLists.txt")
        message("Generating: ${project_dir}")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir})
    endif ()
endforeach ()

set(CMAKE_SUPPRESS_REGENERATION true)







