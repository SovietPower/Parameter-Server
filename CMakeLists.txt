cmake_minimum_required(VERSION 3.17.0)

set(CMAKE_TOOLCHAIN_FILE "E:/Runtime/vcpkg/scripts/buildsystems/vcpkg.cmake")
# 用于 MSVC
# set(VCPKG_TARGET_TRIPLET "x64-windows")
# 用于 MinGW Makefiles
set(VCPKG_TARGET_TRIPLET "x64-mingw-static")

project(ps LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if (MSVC)
	message("in MSVC!")
	add_compile_options(
		/Wall
		/permissive-
		/Zc:__cplusplus
	)
else ()
	message("not in MSVC!")
	add_compile_options(
		-Wall
		-Wpedantic
		-O2
		-Wno-class-memaccess # disable 'Warn when the destination of a call to a raw memory function such as memset or memcpy is an object of class type'
	)
endif()

# 根据模板生成 config.h
# set(VERBOSE 1)
# configure_file (
# 	"${PROJECT_SOURCE_DIR}/config.h.in"
# 	"${PROJECT_SOURCE_DIR}/config.h"
# )

# fmt
# for wsl
# set(fmt_DIR "/mnt/e/Runtime/vcpkg/installed/x64-windows/share/fmt")
find_package(fmt CONFIG REQUIRED)

# ps
add_subdirectory(src) # 生成 ps_lib

# main
add_executable(main main.cpp)
target_include_directories(main PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(main PUBLIC ps_lib)
target_link_libraries(ps_lib PUBLIC fmt::fmt)

# 为 clangd 生成 compile_commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# add_subdirectory(tests)

# include(CTest)
# enable_testing()