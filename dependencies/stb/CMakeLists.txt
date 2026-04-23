cmake_minimum_required(VERSION 3.25)

project(stb LANGUAGES C)

add_library(stb INTERFACE)

target_include_directories(stb INTERFACE include)
