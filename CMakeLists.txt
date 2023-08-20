cmake_minimum_required(VERSION 3.8)
project(PBR-IBL)

set(DATA_DIR ${PROJECT_SOURCE_DIR}/data)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Building in Release mode by default")
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL)

pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(ASSIMP REQUIRED assimp)

set(OPENGL_SRC
    src/application.cpp
    src/application.hpp
    src/image.cpp
    src/image.hpp
    src/main.cpp
    src/mesh.cpp
    src/mesh.hpp
    src/renderer.hpp
    src/utils.cpp
    src/utils.hpp
    src/openglUtility.cpp
    src/openglUtility.hpp
)

set(LIBRARY_SRC
    lib/stb/src/libstb.c
)

set(INCLUDE_DIRS
    lib/glm/include
    lib/stb/include
)

if(OpenGL_FOUND)
    list(APPEND LIBRARY_SRC
        lib/glad/src/glad.c
    )
    list(APPEND INCLUDE_DIRS
        lib/glad/include
    )
    list(APPEND DEFINITIONS 
        ENABLE_OPENGL
    )
endif()

# Add the executable target
add_executable(PBR-IBL ${OPENGL_SRC} ${LIBRARY_SRC})

# Specify compilation options
target_compile_features(PBR-IBL PRIVATE cxx_std_14)
target_compile_definitions(PBR-IBL PRIVATE GLFW_INCLUDE_NONE GLM_ENABLE_EXPERIMENTAL ${DEFINITIONS})

# Specify include directories and libraries
target_include_directories(PBR-IBL PRIVATE 
    ${INCLUDE_DIRS} 
    ${GLFW_INCLUDE_DIRS} 
    ${ASSIMP_INCLUDE_DIRS} 
    ${OPENGL_INCLUDE_DIRS}
)
target_link_libraries(PBR-IBL 
    dl 
    ${GLFW_LIBRARIES} 
    ${ASSIMP_LIBRARIES} 
    ${OPENGL_LIBRARIES}
)

# Install the target
install(TARGETS PBR-IBL DESTINATION ${DATA_DIR})
