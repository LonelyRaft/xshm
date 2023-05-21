cmake_minimum_required(VERSION 3.5)

project(xshm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME xshm)
add_executable(
    ${TARGET_NAME}
    .gitignore
    readme.md
)

file(GLOB SRC_LIST ./*.cpp ./*.c ./*.h)
target_sources(
    ${TARGET_NAME} PRIVATE
    ${SRC_LIST}
)


