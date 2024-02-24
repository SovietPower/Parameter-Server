cmake_minimum_required(VERSION 3.17.0)

project(ps_lib LANGUAGES CXX)

add_library(ps_lib STATIC)
target_include_directories(ps_lib PUBLIC "${CMAKE_SOURCE_DIR}/src")

file(GLOB ps_lib_src
	"${CMAKE_SOURCE_DIR}/src/base/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/kv_app/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/ps/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/utility/*.cpp"
)
target_sources(ps_lib PUBLIC ${ps_lib_src})

target_link_libraries(ps_lib PUBLIC fmt::fmt)

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
		ps_lib
	)
	gtest_discover_tests(${target})
endmacro()

# AddTest(LogTest "base" "log_test")
AddTest(SVectorTest "utility" "SVector_test")
