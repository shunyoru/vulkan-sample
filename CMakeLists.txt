cmake_minimum_required(VERSION 3.20)
project(VulkanTriangle CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 找 Homebrew prefix（支援 Intel 和 Apple Silicon）─────────────────────────
execute_process(
    COMMAND brew --prefix
    OUTPUT_VARIABLE HOMEBREW_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Homebrew prefix: ${HOMEBREW_PREFIX}")

# ── 告訴 CMake 去哪裡找套件 ────────────────────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH
    "${HOMEBREW_PREFIX}/opt/molten-vk"
    "${HOMEBREW_PREFIX}/opt/vulkan-loader"
    "${HOMEBREW_PREFIX}/opt/vulkan-headers"
    "${HOMEBREW_PREFIX}/opt/glfw"
    "${HOMEBREW_PREFIX}/opt/glm"
)

# ── 尋找依賴 ───────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# ── 執行檔 ────────────────────────────────────────────────────────────────
add_executable(VulkanTriangle src/main.cpp)

target_include_directories(VulkanTriangle PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    "${HOMEBREW_PREFIX}/opt/glm/include"
)

target_link_libraries(VulkanTriangle PRIVATE
    Vulkan::Vulkan
    glfw
)

# ── 自動編譯 Shader ────────────────────────────────────────────────────────
find_program(GLSLC glslc HINTS "${HOMEBREW_PREFIX}/opt/shaderc/bin")

if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")

    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/shaders/shader.vert.spv"
        COMMAND ${GLSLC} "${CMAKE_SOURCE_DIR}/shaders/shader.vert"
                -o "${CMAKE_SOURCE_DIR}/shaders/shader.vert.spv"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/shader.vert"
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/shaders/shader.frag.spv"
        COMMAND ${GLSLC} "${CMAKE_SOURCE_DIR}/shaders/shader.frag"
                -o "${CMAKE_SOURCE_DIR}/shaders/shader.frag.spv"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/shader.frag"
        COMMENT "Compiling fragment shader"
    )
    add_custom_target(Shaders ALL
        DEPENDS
            "${CMAKE_SOURCE_DIR}/shaders/shader.vert.spv"
            "${CMAKE_SOURCE_DIR}/shaders/shader.frag.spv"
    )
    add_dependencies(VulkanTriangle Shaders)
else()
    message(WARNING "glslc not found. Shaders will not be compiled automatically.")
endif()
