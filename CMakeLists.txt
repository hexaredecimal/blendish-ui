cmake_minimum_required(VERSION 3.10)

project(blendish LANGUAGES C CXX)

set(SOURCES
    example.cpp
    external/nanovg/nanovg.c
)

add_executable(example
    ${SOURCES}
)

target_include_directories(example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    external/nanovg
)

if(WIN32)

    target_compile_definitions(example PRIVATE
        NANOVG_GLEW
    )

    target_link_libraries(example PRIVATE
        glfw3
        glew32
        opengl32
        glu32
        gdi32
        winmm
        user32
    )

elseif(APPLE)

    find_library(COCOA_LIB Cocoa)
    find_library(OPENGL_LIB OpenGL)
    find_library(IOKIT_LIB IOKit)
    find_library(COREVIDEO_LIB CoreVideo)

    target_link_libraries(example PRIVATE
        glfw3
        ${COCOA_LIB}
        ${OPENGL_LIB}
        ${IOKIT_LIB}
        ${COREVIDEO_LIB}
    )

else()

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GLFW REQUIRED glfw3)
    pkg_check_modules(GLEW REQUIRED glew)

    target_include_directories(example PRIVATE
        ${GLFW_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
    )

    target_link_directories(example PRIVATE
        ${GLFW_LIBRARY_DIRS}
        ${GLEW_LIBRARY_DIRS}
    )

    target_link_libraries(example PRIVATE
        ${GLFW_LIBRARIES}
        ${GLEW_LIBRARIES}
        GL
        GLU
        m
    )

    target_compile_definitions(example PRIVATE
        NANOVG_GLEW
    )

endif()

target_compile_definitions(example PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)