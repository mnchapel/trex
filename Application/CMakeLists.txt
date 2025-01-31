cmake_minimum_required(VERSION 3.0)
if(${CMAKE_VERSION} VERSION_LESS "3.11.0")
    message(STATUS "* Please consider to switch to CMake 3.11.0")
endif()

message("-- USER environment variable is set to: " $ENV{USER})

project(TRex)
if(POLICY CMP0114)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0114 NEW)
endif()
set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
include(ExternalProject)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.12.0")
    include(FetchContent)
endif()

enable_testing ()

set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.tristan.tracker")
set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "tristan")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
set(CMAKE_XCODE_ATTRIBUTE_GCC_INCREASE_PRECOMPILED_HEADER_SHARING "YES")
set(CMAKE_XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES")

#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
option(TREX_NO_PYTHON OFF)
option(TREX_BUILD_DOCS OFF)
option(TREX_WITH_TESTS OFF)
option(TREX_CONDA_PACKAGE_INSTALL OFF)
option(WITH_GITSHA1 "include git hash" ON)

if(COMMONS_NO_PYTHON)
    set(TREX_NO_PYTHON ON)
endif()

set(CONDA_PREFIX $ENV{CONDA_PREFIX} CACHE PATH "CONDA_PREFIX if set during cmake")
if(CONDA_PREFIX)
    message(STATUS "CONDA_PREFIX=${CONDA_PREFIX}")
endif()

if(TREX_CONDA_PACKAGE_INSTALL)
    add_definitions(-DTREX_CONDA_PACKAGE_INSTALL=1)
    set(CMAKE_BUILD_RPATH "${CONDA_PREFIX}/lib")
    
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    endif("${isSystemDir}" STREQUAL "-1")
endif()

add_subdirectory(src/commons)

set(CMAKE_CXX_STANDARD 20)
message(STATUS "Using c++20 standard (${CMAKE_CXX_STANDARD}).")

set(cxx_flag_std_number "")
if(CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-std=c++2a HAVE_FLAG_STD_CXX20)
    if(HAVE_FLAG_STD_CXX20)
        message(STATUS "Using support for C++20")
        set(CMAKE_CXX_STANDARD 20)
        set(cxx_flag_std_number "-std=c++2a")
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${cxx_flag_std_number}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${cxx_flag_std_number}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${cxx_flag_std_number}")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
string( TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE )
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;DEBUG;")

if(APPLE)
    if(CMAKE_OSX_DEPLOYMENT_TARGET AND NOT ${CMAKE_OSX_DEPLOYMENT_TARGET} MATCHES "")
        message(STATUS "Found OSX deployment target: '${CMAKE_OSX_DEPLOYMENT_TARGET}'")
        set(TREX_MACOS_VERSION ${CMAKE_OSX_DEPLOYMENT_TARGET})
    else()
        message(STATUS "Need to detect OSX version.")
        if(NOT CMAKE_OSX_SYSROOT)
            message(FATAL_ERROR "No OSX sysroot was set.")
        else()
            get_filename_component(osx_dir ${CMAKE_OSX_SYSROOT} NAME)
            string(REPLACE ".sdk" "" osx_dir ${osx_dir})
            string(REPLACE "MacOSX" "" osx_dir ${osx_dir})
            message(STATUS "OSX Target: ${osx_dir}")
            set(TREX_MACOS_VERSION ${osx_dir})
        endif()
    endif()
endif()



set(CMAKE_DEBUG_POSTFIX -d)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
endif()
set(ORIGINAL_CXX_FLAGS ${CMAKE_CXX_FLAGS})
message(STATUS "ORIGINAL_CXX_FLAGS=${ORIGINAL_CXX_FLAGS}")

if(WIN32)
    set(BUILD_SHARED_LIBS OFF)
    string(REPLACE " " ";" VERSION_LIST ${CMAKE_GENERATOR})
    list(GET VERSION_LIST 2 VSTD_VERSION_MAJOR)
    
    math (EXPR MSVC_TOOLSET_MAJOR "${MSVC_VERSION}/10")
    message(STATUS "CMAKE_GENERATOR_TOOLSET ${MSVC_TOOLSET_MAJOR} ${CMAKE_GENERATOR} ${VSTD_VERSION_MAJOR}")
    
    #if(${MSVC_TOOLSET_MAJOR} LESS 190)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_ENABLE_ATOMIC_ALIGNMENT_FIX")
    #endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_USE_MATH_DEFINES -DNOMINMAX -DCURL_STATICLIB")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /bigobj")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd /bigobj")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT /bigobj /O2 /Oi /Oy /Ot /Ob2 /Zi")
    
    set(GLEW_STATIC ON)
else()
    set(BUILD_SHARED_LIBS OFF)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten") #-pthread -s PTHREAD_POOL_SIZE=8  -s TOTAL_MEMORY=250MB
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -O3 -s FETCH=1 -s ALLOW_MEMORY_GROWTH=0 -s WASM=1 -s FULL_ES3=1 -s USE_GLFW=3 -s LEGACY_GL_EMULATION=0 -s \"BINARYEN_METHOD='native-wasm'\" -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s PTHREAD_POOL_SIZE=4 -s GL_PREINITIALIZED_CONTEXT=1") # 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -pthread")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file fonts --preload-file group_1.pv -s TOTAL_MEMORY=1000MB")
    set(CMAKE_CONFIGURATION_TYPES "Release" FORCE)
endif()

set(TREX_CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
set(TREX_CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

# Set output paths
set(PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake" )
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DTT_DEBUG_ENABLED")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(TREX_CONDA_PACKAGE_INSTALL)
    set(TREX_CMAKE_BUILD_TYPE Release)
else()
    if(APPLE)
        set(TREX_CMAKE_BUILD_TYPE $<$<CONFIG:Debug>:Release>$<$<CONFIG:Release>:Debug>)
    else()
        set(TREX_CMAKE_BUILD_TYPE $<IF:$<CONFIG:Debug>,Debug,Release>)
    endif()
endif()

#if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    #set(C_SANITIZE_OPTIONS "-fsanitize=thread,signed-integer-overflow,null,alignment,undefined,bounds -fno-sanitize-recover=null -fsanitize-trap=alignment,undefined,signed-integer-overflow")
    set(C_SANITIZE_OPTIONS "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-stack-address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 ${C_SANITIZE_OPTIONS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 ${C_SANITIZE_OPTIONS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic -Wall -Wextra -pedantic")
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=return-stack-address")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 ${C_SANITIZE_OPTIONS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 ${C_SANITIZE_OPTIONS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-c++98-compat-pedantic")
    message(STATUS "hidden visibility in main project ${CMAKE_CXX_FLAGS}")
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-nullability-extension")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-nullability-extension")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${C_SANITIZE_OPTIONS}")
elseif(MSVC)
else()
    message(STATUS "***** compiler ID ${CMAKE_CXX_COMPILER_ID}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-local-addr")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wno-long-long -ggdb -DOM_DEBUG -O0 -no-pie")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0 -no-pie")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -ggdb -no-pie")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -no-pie -fPIC -Werror=return-local-addr")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g0")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -ggdb")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pedantic -Wall -Wextra -Wno-long-long -ggdb -DOM_DEBUG -O0")
endif()

if(XCODE)
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
    
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_RETURN_TYPE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_ABOUT_MISSING_NEWLINE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VALUE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_VARIABLE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_SIGN_COMPARE "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNUSED_FUNCTION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_INITIALIZER_NOT_FULLY_BRACKETED "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_HIDDEN_VIRTUAL_FUNCTIONS "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_UNINITIALIZED_AUTOS "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_BOOL_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_EMPTY_BODY "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_ENUM_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_INT_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_WARN_CONSTANT_CONVERSION "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_WARN_NON_VIRTUAL_DESTRUCTOR "YES")
endif()

set(trex_subproject_CMAKE_ARGS "")
message(STATUS "TREX PYTHON VARS: lib:${PYTHON_LIBRARY} inc:${PYTHON_INCLUDE_DIR} exec:${PYTHON_EXECUTABLE}")

SET(trex_subproject_CMAKE_ARGS ${trex_subproject_CMAKE_ARGS}
    -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
    -DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
    -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
    -DCMAKE_C_FLAGS_RELEASE:STRING=${CMAKE_C_FLAGS_RELEASE}
    -DCMAKE_C_FLAGS_RELWITHDEBINFO:STRING=${CMAKE_C_FLAGS_RELWITHDEBINFO}
    -DCMAKE_C_FLAGS_DEBUG:STRING=${CMAKE_C_FLAGS_DEBUG}
    #-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_CXX_FLAGS=${ORIGINAL_CXX_FLAGS}
    -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
    -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
    -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    -DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF 
    -DSKIP_INSTALL_FILES=1
    -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
    -DPYTHON_INCLUDE_DIR:PATH=${PYTHON_INCLUDE_DIR}
    -DPYTHON_LIBRARY:PATH=${PYTHON_LIBRARY}
    -DPYTHON_EXECUTABLE:FILEPATH=${PYTHON_EXECUTABLE}
)

if(APPLE)
    set(trex_subproject_CMAKE_ARGS ${trex_subproject_CMAKE_ARGS}
        -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

#if(APPLE AND CMAKE_COMPILER_IS_GNUCXX)
#  set(trex_subproject_CMAKE_ARGS "-DWITH_AVFOUNDATION=OFF" "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}")
#endif()

message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET = ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "MACOSX_DEPLOYMENT_TARGET = $ENV{MACOSX_DEPLOYMENT_TARGET}")


if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    set_property (TARGET imgui APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc -D'NS_FORMAT_ARGUMENT(A)=' -D_Nullable_result=_Nullable")
endif()


#find_package(commons)
include_directories(src/commons)
include_directories(src/commons/common)


add_subdirectory(src)

include_directories(${CMAKE_SOURCE_DIR}/src/)
include_directories(${CMAKE_SOURCE_DIR}/cpputils/src/)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()


if(NOT WIN32 AND ${TREX_BUILD_DOCS})
    message(STATUS "Searching Sphinx at ${CONDA_PREFIX} ${CONDA_PREFIX}/bin")
    find_package(Sphinx REQUIRED) #PATHS ${CONDA_PREFIX} ${CONDA_PREFIX}/bin )
    if(NOT Sphinx_FOUND)
        message(ERROR "Cannot find Sphinx executable")
    else()
        get_filename_component(SPHINX_DIR ${SPHINX_EXECUTABLE} DIRECTORY)
        message(STATUS "Found sphinx at ${SPHINX_EXECUTABLE} (dir: ${SPHINX_DIR}).")

        add_custom_target(
          doc ALL
          COMMAND $<TARGET_FILE:TRex> -d "${CMAKE_SOURCE_DIR}/../docs" -h rst
          COMMAND $<TARGET_FILE:TGrabs> -d "${CMAKE_SOURCE_DIR}/../docs" -h rst
          COMMAND PATH=$ENV{PATH}:${SPHINX_DIR} make html
          DEPENDS
            TRex
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../docs
          COMMENT "Generating API documentation with Doxygen"
          VERBATIM
        )
    endif()
elseif(${TREX_BUILD_DOCS})
    message(STATUS "Searching Sphinx at ${CONDA_PREFIX} ${CONDA_PREFIX}/bin")
    find_package(Sphinx REQUIRED) #PATHS ${CONDA_PREFIX} ${CONDA_PREFIX}/bin )
    if(NOT Sphinx_FOUND)
        message(ERROR "Cannot find Sphinx executable")
    else()
        get_filename_component(SPHINX_DIR ${SPHINX_EXECUTABLE} DIRECTORY)
        message(STATUS "Found sphinx at ${SPHINX_EXECUTABLE} (dir: ${SPHINX_DIR}).")
        message(STATUS "$<TARGET_FILE:tgrabs> -d \"${CMAKE_SOURCE_DIR}/../docs\" -h rst")
        add_custom_target(
            #C:\Users\tristan\Videos\f9_f10_f11_emptytank_20180220_120156.pv
          doc ALL #-i Z:\work\f9_f10_f11_emptytank_20180220_120156\%256d.mp4 -averaging_method mode -threshold 30 -use_closing -closing_size 5 -correct_luminance
          COMMAND ${CMAKE_SOURCE_DIR}/../docs/make.bat html "${SPHINX_DIR}" "${CMAKE_SOURCE_DIR}/../docs" "$<TARGET_FILE:trex>" "$<TARGET_FILE:tgrabs>"
          DEPENDS
            trex tgrabs
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../docs
          COMMENT "Generating API documentation with Doxygen PATH=$ENV{PATH}:${SPHINX_DIR} ${CMAKE_SOURCE_DIR}/../docs/make.bat"
          VERBATIM
        )
    endif()
endif()

if(POLICY CMP0114)
  cmake_policy(POP)
endif()
