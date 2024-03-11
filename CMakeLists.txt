cmake_minimum_required(VERSION 3.17.0)

if (NOT WSL) # 通过 -DWSL=1 设置
	message("not use wsl")
	set(CMAKE_TOOLCHAIN_FILE "E:/Runtime/vcpkg/scripts/buildsystems/vcpkg.cmake")
	# 用于 MSVC
	# set(VCPKG_TARGET_TRIPLET "x64-windows")
	# 用于 MinGW Makefiles
	set(VCPKG_TARGET_TRIPLET "x64-mingw-static")
else()
	message("use wsl")
endif()

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

if (WSL) # 通过 -DWSL=1 设置
	# for wsl
	set(fmt_DIR "/mnt/e/Runtime/vcpkg/installed/x64-windows/share/fmt")
	set(ZeroMQ_DIR "/mnt/e/Runtime/vcpkg/installed/x64-windows/share/zeromq")
	set(protobuf_DIR "/mnt/e/Runtime/vcpkg/installed/x64-windows/share/protobuf")
endif()

# import fmt
find_package(fmt CONFIG REQUIRED)

# import zmq
find_package(ZeroMQ CONFIG REQUIRED)

# import protobuf
find_package(protobuf CONFIG REQUIRED)

# ps
# add_subdirectory(src) # 生成 ps_lib

# --- ps_lib
add_library(ps_lib STATIC)
target_include_directories(ps_lib PUBLIC "${CMAKE_SOURCE_DIR}/src")

file(GLOB ps_lib_src
	"${CMAKE_SOURCE_DIR}/src/base/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/internal/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/ps/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/utility/*.cpp"

	"${CMAKE_SOURCE_DIR}/src/internal/meta.pb.cc"
)
target_sources(ps_lib PUBLIC ${ps_lib_src})
message("CMAKE_SOURCE_DIR: " ${CMAKE_SOURCE_DIR})
message("ps_lib_src: " ${ps_lib_src})

target_link_libraries(ps_lib PRIVATE fmt::fmt)
target_link_libraries(ps_lib PUBLIC libzmq libzmq-static) # 需要 PUBLIC 不然会在链接 SVectorTest 时出错？
target_link_libraries(ps_lib PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)

# Generate protobuf headers and sources
# 好像并不会生成，所以选择用 protoc 手动生成，然后放到 src 目录下
# file(GLOB_RECURSE PROTO_FILES "${CMAKE_SOURCE_DIR}/src/internal/*.proto")
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
# target_sources(ps_lib PRIVATE ${PROTO_SRCS})
# target_include_directories(ps_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) # PROTO_HDRS files are here

# Generate protobuf python interface
# protobuf_generate_python(PROTO_PYS ${PROTO_FILES})
# add_custom_target(proto_python ALL DEPENDS ${PROTO_PYS})
# add_dependencies(ps_lib proto_python)

# --- ps_lib test
# setup gtest
include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
# 允许通过 ctest 一键运行测试
include(CTest)
include(GoogleTest)

# tests
macro (AddTest target folder testFileName)
	# message("full path is " "${CMAKE_SOURCE_DIR}/src/${folder}/test/${testFileName}.cc")
	add_executable(${target} "${CMAKE_SOURCE_DIR}/src/${folder}/test/${testFileName}.cpp")
	target_link_libraries(${target} PRIVATE
		GTest::gtest_main
		# ps_lib # SVector 使用本地相对路径包含文件，就不需要链接和添加 include path 了
	)
	gtest_discover_tests(${target})
endmacro()

# AddTest(LogTest "base" "Log_test")
AddTest(SVectorTest "utility" "SVector_test")

# --- ps_lib test end
# --- ps_lib end

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