set_property(GLOBAL PROPERTY CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Check for standard to use
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++17 SUPPORT_STD_CXX17)
check_cxx_compiler_flag(-std=c++14 SUPPORT_STD_CXX14)

# Check ROOT
set(CMAKE_PREFIX_PATH $ENV{ROOTSYS})
set(ROOT_DIR $ENV{ROOTSYS}/cmake)
find_package(ROOT QUIET)
if(${ROOT_FOUND})
  if(ROOT_USE_FILE)
    include(${ROOT_USE_FILE})
  endif()
  # Downgrade to C++14 if ROOT is not build with C++17 support
  #IF(ROOT_CXX_FLAGS MATCHES ".*std=c\\+\\+1[7z].*")
  #  IF(NOT SUPPORT_STD_CXX17)
  #    MESSAGE(FATAL_ERROR "ROOT was built with C++17 support but current compiler doesn't support it")
  #  ENDIF()
  #ELSEIF(ROOT_CXX_FLAGS MATCHES ".*std=c\\+\\+1[14y].*")
  #  SET(CMAKE_CXX_STANDARD 14)
  #ELSEIF(ROOT_CXX_FLAGS MATCHES ".*std=c\\+\\+.*")
  #  MESSAGE(FATAL_ERROR "ROOT was built with an unsupported C++ version: ${ROOT_CXX_FLAGS}")
  #ELSE()
  #  MESSAGE(FATAL_ERROR "Could not deduce ROOT's C++ version from build flags: ${ROOT_CXX_FLAGS}")
  #ENDIF()


  # Force C++17 if supported by the compiler
  if(SUPPORT_STD_CXX17)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "Forcing C++17 standard")
  elseif(SUPPORT_STD_CXX14)
    set(CMAKE_CXX_STANDARD 14)
    message(STATUS "Forcing C++14 standard")
  else()
    message(FATAL_ERROR "Neither C++17 nor C++14 are supported by the compiler")
  endif()


else()
  # Try activating the highest compiler standard available
  if(SUPPORT_STD_CXX17)
    set(CMAKE_CXX_STANDARD 17)
  else()
    if(SUPPORT_STD_CXX14)
      set(CMAKE_CXX_STANDARD 14)
    endif()
  endif()
endif()


set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
  if(NOT MSVC)
    message(FATAL_ERROR "On Microsoft Windows, only Visual Studio is support")
  endif()
  add_definitions("-DEUDAQ_PLATFORM=PF_WIN32")
  add_definitions("-DEUDAQ_FUNC=__FUNCTION__")
  add_definitions("/wd4251") # disables warning concerning dll-interface (comes up for std classes too often)
  add_definitions("/wd4996") # this compiler warnung is about that functions like fopen are unsafe.
  add_definitions("/wd4800") # disables warning concerning usage of old style bool (in root)
elseif(APPLE)
  if(NOT ((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
    message(FATEL ERROR "On Unix/Linux like system, only GCC and Clang is support")
  endif()
  add_definitions("-DEUDAQ_PLATFORM=PF_MACOSX")
  add_definitions("-DEUDAQ_FUNC=__PRETTY_FUNCTION__ ")
  list(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,-undefined,error")
else()
  if(NOT ((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
    message(FATEL ERROR "On Unix/Linux like system, only GCC and Clang is support")
  endif()
  add_definitions("-DEUDAQ_PLATFORM=PF_LINUX")
  add_definitions("-DEUDAQ_FUNC=__PRETTY_FUNCTION__ ")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    #workaround for xfer_bufptrs public/private bug
    #http://mailman.isi.edu/pipermail/ns-developers/2016-February/013378.html
    #https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69470
    add_definitions("-include sstream")
  endif()  
  list(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
endif()
