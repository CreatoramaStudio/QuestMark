# Jolt vs PhysX articulated-body tests. See README.md.
# The library sources are NOT in this repository. Point the cache variables at local clones, for example
#   cmake -S benchmark/physics-articulated -B <build dir> -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#         -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DJOLT_DIR=<jolt clone> -DPHYSX_DIR=<PhysX clone>
#   cmake --build <build dir>
# Jolt and PhysX are built once here and shared by the four test folders.
cmake_minimum_required(VERSION 3.28)
project(physics_articulated LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JOLT_DIR "" CACHE PATH "Jolt Physics source tree (the folder that contains Build/CMakeLists.txt)")
set(PHYSX_DIR "" CACHE PATH "NVIDIA PhysX source tree (the folder that contains physx/CMakeLists.txt)")

if(NOT JOLT_DIR AND NOT PHYSX_DIR)
  message(FATAL_ERROR "set JOLT_DIR and/or PHYSX_DIR")
endif()

# One CRT for everything (dynamic, /MD): Jolt and PhysX default to different ones.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

function(artic_target_defaults target)
  if(MSVC) # also true for clang-cl
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# timing helper (header only)
add_library(artic_bench INTERFACE)
target_include_directories(artic_bench INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common)

# ufbx (vendored, MIT or Unlicense): reads the humanoid skeleton FBX
add_library(artic_ufbx STATIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ufbx/ufbx.c)
target_include_directories(artic_ufbx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ufbx)
set_target_properties(artic_ufbx PROPERTIES C_STANDARD 99)

add_library(artic_jolt_dep INTERFACE)
add_library(artic_physx_dep INTERFACE)

if(JOLT_DIR)
  set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
  set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
  set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
  set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
  set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
  add_subdirectory(${JOLT_DIR}/Build ${CMAKE_CURRENT_BINARY_DIR}/jolt EXCLUDE_FROM_ALL)
  target_link_libraries(artic_jolt_dep INTERFACE Jolt)
  target_include_directories(artic_jolt_dep INTERFACE ${JOLT_DIR})
endif()

if(PHYSX_DIR)
  # CPU only, static libraries, no snippets, no GPU/CUDA, dynamic CRT. PhysX writes include/PxConfig.h into its own tree.
  set(PX_GENERATE_GPU_PROJECTS OFF CACHE BOOL "" FORCE)
  set(PX_GENERATE_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
  set(PX_BUILDSNIPPETS OFF CACHE BOOL "" FORCE)
  set(PX_BUILDPVDRUNTIME OFF CACHE BOOL "" FORCE)
  set(NV_USE_STATIC_WINCRT OFF CACHE BOOL "" FORCE)
  # PhysX knows the configs debug/checked/profile/release only; under RelWithDebInfo it would define none of its per-config
  # macros. Define the ones of its "release" config (PVD off) by hand, for the PhysX tree and for our backend files alike.
  # clang-cl: PhysX passes /MP and /WX, which clang-cl rejects (unused argument as error), so drop /WX and the warning.
  get_directory_property(_old_opts COMPILE_OPTIONS)
  get_directory_property(_old_defs COMPILE_DEFINITIONS)
  add_compile_options(-Qunused-arguments /WX-)
  add_compile_definitions(PX_SUPPORT_PVD=0 PX_SUPPORT_OMNI_PVD=0)
  add_subdirectory(${PHYSX_DIR}/physx ${CMAKE_CURRENT_BINARY_DIR}/physx EXCLUDE_FROM_ALL)
  set_directory_properties(PROPERTIES COMPILE_OPTIONS "${_old_opts}" COMPILE_DEFINITIONS "${_old_defs}")
  # The PhysX .rc version-resource files are UTF-16, which llvm-rc cannot read, and are useless in a static library: drop them.
  foreach(_t PhysX PhysXCommon PhysXFoundation PhysXExtensions PhysXPvdSDK PhysXCharacterKinematic PhysXVehicle)
    if(TARGET ${_t})
      get_target_property(_srcs ${_t} SOURCES)
      list(FILTER _srcs EXCLUDE REGEX "\\.rc$")
      set_property(TARGET ${_t} PROPERTY SOURCES ${_srcs})
    endif()
  endforeach()
  target_link_libraries(artic_physx_dep INTERFACE PhysX PhysXExtensions PhysXPvdSDK PhysXCommon PhysXFoundation)
  target_include_directories(artic_physx_dep INTERFACE ${PHYSX_DIR}/physx/include)
  target_compile_definitions(artic_physx_dep INTERFACE PX_PHYSX_STATIC_LIB PX_SUPPORT_PVD=0 PX_SUPPORT_OMNI_PVD=0)
endif()

add_subdirectory(chain-limb)
add_subdirectory(standing-foot)
add_subdirectory(hip-twist)
add_subdirectory(full-body)
