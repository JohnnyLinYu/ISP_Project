cmake_minimum_required(VERSION 3.16)
project(ISP_Pipeline CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)

add_executable(hello src/main.cpp)
add_executable(Unittest 
Unittest/testCamera.cpp 
src/thread_pool.cpp
)
target_include_directories(Unittest PRIVATE include)
target_link_libraries(hello PRIVATE 
	${OpenCV_LIBS}
	PkgConfig::LIBCAMERA
)
target_link_libraries(Unittest PRIVATE 
	${OpenCV_LIBS}
	PkgConfig::LIBCAMERA
)
