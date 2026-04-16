cmake_minimum_required(VERSION 3.10)
project(utrom_reflex_g1)

# 1. ORIN NX ARCHITECTURE OPTIMIZATIONS
# armv8.2-a is the base for Orin. +simd and +fp16 are critical for NEON neural kernels.
set(CMAKE_CXX_STANDARD 17)
set(ORIN_FLAGS "-O3 -march=armv8.2-a+fp16+simd -mtune=cortex-a78ae -funsafe-math-optimizations -ffast-math")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ORIN_FLAGS}")

# 2. PATHS & DIRECTORIES
set(UTROM_CORE_INC "${CMAKE_CURRENT_SOURCE_DIR}/src/utromfx_core/include")
set(REFLEX_INC "${CMAKE_CURRENT_SOURCE_DIR}/src/reflex_g1/include")
set(GENERATED_INC "${CMAKE_CURRENT_SOURCE_DIR}") # Where G1Controller.h lives

# 3. UNITREE SDK DEPENDENCIES
# Note: Ensure UNITREE_SDK_PATH is set in your environment
find_package(unitree_sdk REQUIRED) 

# 4. EXECUTABLE DEFINITION
add_executable(utrom_g1_node
    src/reflex_g1/main_g1.cpp
)

# 5. INCLUDE DIRECTORIES
target_include_directories(utrom_g1_node PRIVATE
    ${UTROM_CORE_INC}
    ${REFLEX_INC}
    ${GENERATED_INC}
    ${unitree_sdk_INCLUDE_DIRS}
)

# 6. LINK LIBRARIES
target_link_libraries(utrom_g1_node
    PRIVATE
    ${unitree_sdk_LIBRARIES}
    pthread
    rt # For real-time clock and shm
)

# 7. STRIP BINARY (Optional but recommended for deployment)
# Reduces binary size and improves load times on embedded systems
add_custom_command(TARGET utrom_g1_node POST_BUILD
    COMMAND ${CMAKE_STRIP} utrom_g1_node
    COMMENT "Stripping symbols from final robot binary..."
)