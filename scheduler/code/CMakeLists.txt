cmake_minimum_required(VERSION 3.7)
project(scheduler)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ldl")

#set(BOOST_VERSION Boost-1.79.0)
set(BOOST_VERSION Boost-1.78.0)
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "$HOME/boost/include")
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "$HOME/boost/lib")
set(BOOST_ROOT  $ENV{HOME}/boost)
set(BOOST_INCLUDEDIR $ENV{HOME}/boost/include)
set(BOOST_LIBRARYDIR $ENV{HOME}/boost/lib)
set(Boost_DIR $ENV{HOME}/boost/lib/cmake/${BOOST_VERSION})

add_library(cplex-library STATIC IMPORTED GLOBAL)
set_target_properties(cplex-library PROPERTIES
        IMPORTED_LOCATION "$ENV{CPLEX_HOME}/cplex/lib/x86-64_linux/static_pic/libcplex.a"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{CPLEX_HOME}/cplex/include")

add_library(cplex-concert STATIC IMPORTED GLOBAL)
set_target_properties(cplex-concert PROPERTIES
        IMPORTED_LOCATION "$ENV{CPLEX_HOME}/concert/lib/x86-64_linux/static_pic/libconcert.a"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{CPLEX_HOME}/concert/include"
        INTERFACE_COMPILE_DEFINITIONS IL_STD)

add_library(ilocplex STATIC IMPORTED GLOBAL)
set_target_properties(ilocplex PROPERTIES
        IMPORTED_LOCATION "$ENV{CPLEX_HOME}/cplex/lib/x86-64_linux/static_pic/libilocplex.a"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{CPLEX_HOME}/cplex/include"
        INTERFACE_LINK_LIBRARIES "cplex-concert;cplex-library")

find_package(nlohmann_json 3.7.0 REQUIRED)

find_package(Boost CONFIG REQUIRED COMPONENTS serialization)
IF (BOOST_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS("-DHAS_BOOST")
ENDIF()

add_executable(scheduler scheduler.cpp FileReader.cpp FileReader.h Utils.cpp Utils.h Output.cpp Output.h Parser.cpp Parser.h DataStructures.cpp DataStructures.h)

target_link_libraries(scheduler PRIVATE ilocplex cplex-library cplex-concert -lpthread -lm ${Boost_LIBRARIES} nlohmann_json::nlohmann_json -ldl)


