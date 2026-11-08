@echo off
rem Helper of benchmark/tools/setup-physx.sh: PhysX 5.10 SDK with clang-cl, static CRT, CPU only, release.
rem Same cmake command line as the dev machine's phase2-physx.bat (clang-cl route, see benchmark/godot/PHYSX.md "Build route: PhysX SDK with clang-cl").
rem Takes everything from environment variables set by the caller (no machine paths here):
rem   PX_VCVARS   full path of vcvars64.bat (Visual Studio 2022)
rem   PX_LLVMBIN  LLVM bin folder (clang-cl), PX_NINJADIR  folder with ninja.exe (empty = already on PATH)
rem   PXR         <PhysX clone>/physx (forward slashes)   PX_BLD  CMake build folder (forward slashes)   PX_JOBS  parallel jobs
call "%PX_VCVARS%" >nul 2>&1
if errorlevel 1 (echo vcvars64.bat failed & exit /b 1)
set "PATH=%PX_LLVMBIN%;%PX_NINJADIR%;%PATH%"
cmake --no-warn-unused-cli -S %PXR%/compiler/public -B %PX_BLD% -G "Ninja Multi-Config" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_RC_COMPILER=rc -DCMAKE_NINJA_CMCLDEPS_RC=OFF -DCMAKE_SIZEOF_VOID_P=8 -DTARGET_BUILD_PLATFORM=windows -DPX_OUTPUT_ARCH=x86 -DPHYSX_ROOT_DIR=%PXR% -DPX_OUTPUT_LIB_DIR=%PXR% -DPX_OUTPUT_BIN_DIR=%PXR% -DPX_BUILDSNIPPETS=OFF -DPX_BUILDPVDRUNTIME=OFF -DPX_GENERATE_STATIC_LIBRARIES=ON -DPX_CMAKE_SUPPRESS_REGENERATION=ON -DPX_GENERATE_GPU_PROJECTS=OFF -DNV_USE_STATIC_WINCRT=ON -DNV_USE_DEBUG_WINCRT=OFF -DPX_FLOAT_POINT_PRECISE_MATH=OFF -DCMAKE_INSTALL_PREFIX=%PXR%/install/vc17clang-godot/PhysX || exit /b 1
cmake --build %PX_BLD% --config release --parallel %PX_JOBS% || exit /b 1
cmake --install %PX_BLD% --config release || exit /b 1
echo SDK: %PXR%/install/vc17clang-godot/PhysX
