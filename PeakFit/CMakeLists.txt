cmake_minimum_required(VERSION 3.16)
project(PeakFitScience)

set(CMAKE_CXX_STANDARD 17)
find_package(ROOT REQUIRED)

include_directories(include ${ROOT_INCLUDE_DIRS})

file(GLOB SRC src/*.cpp app/*.cpp)

add_executable(peakfit ${SRC})

target_link_libraries(peakfit PRIVATE ${ROOT_LIBRARIES})
