cmake_minimum_required(VERSION 2.8)
project (jsoncpp)
add_library(jsoncpp STATIC jsoncpp.cpp json/json.h json/json-forwards.h)
