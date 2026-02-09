# FindAvisynthPlus.cmake - Finds the Avisynth+ headers
#
# This module finds the correct include directory so that C++ code can
# consistently use `#include <avisynth.h>`.
#
# It respects the standard CMAKE_PREFIX_PATH variable.
# To specify a custom SDK location, run cmake with:
#   -D CMAKE_PREFIX_PATH="C:/path/to/your/sdk"
#
# As a fallback, it also respects the AVISYNTHPLUS_ROOT_DIR variable.
#
# It defines the following imported target:
#   AvisynthPlus::headers  - An INTERFACE target providing the include directory.

include(FindPackageHandleStandardArgs)

# 1. Find the directory that DIRECTLY contains avisynth.h or avisynth_c.h
#    We use PATH_SUFFIXES to check common layouts.
find_path(AvisynthPlus_INCLUDE_DIR
  NAMES
    avisynth.h
    avisynth_c.h
  HINTS
    # Allow a specific root directory to be provided as a hint
    ENV AVISYNTHPLUS_ROOT
    ${AVISYNTHPLUS_ROOT_DIR}/include
    "C:/Program Files (x86)/AviSynth+/FilterSDK/include"
    /usr/local/include
  PATH_SUFFIXES
    # For namespaced layouts like /usr/include/avisynth/avisynth.h
    include/avisynth
    sdk/include/avisynth
    # For flat layouts like C:/sdk/include/avisynth.h
    include
    sdk/include
)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64‑bit
    set(DEFAULT_LIB_DIR_WIN32 "C:/Program Files (x86)/AviSynth+/FilterSDK/lib/x64")
else()
    # 32‑bit
    set(DEFAULT_LIB_DIR_WIN32 "C:/Program Files (x86)/AviSynth+/FilterSDK/lib/x86")
endif()


find_path(AvisynthPlus_LIB_DIR
  NAMES
    avisynth.lib
    avisynth.dll
    libavisynth.so
    libavisynth.dylib
  HINTS
    # Allow a specific root directory to be provided as a hint
    ENV AVISYNTHPLUS_ROOT
    ${AVISYNTHPLUS_ROOT_DIR}
    ${DEFAULT_LIB_DIR_WIN32}
    /usr/local/lib
)

# 2. Handle the standard arguments for find_package
find_package_handle_standard_args(AvisynthPlus
  FOUND_VAR AvisynthPlus_FOUND
  REQUIRED_VARS
    AvisynthPlus_INCLUDE_DIR
    AvisynthPlus_LIB_DIR
  FAIL_MESSAGE
    "Could not find Avisynth+ headers (avisynth.h/avisynth_c.h) or library (avisynth.lib/libavisynth.so). Please specify the SDK location by setting CMAKE_PREFIX_PATH or AVISYNTHPLUS_ROOT_DIR."
)

# 3. If found, create the INTERFACE imported target
if(AvisynthPlus_FOUND)
  if(NOT TARGET AvsCore)
    # If the target doesn't exist at all, create the real IMPORTED target.
    add_library(AvsCore INTERFACE IMPORTED)
    set_target_properties(AvsCore PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${AvisynthPlus_INCLUDE_DIR}"
    )
    if(EXISTS "${AvisynthPlus_LIB_DIR}/avisynth.lib")
        set(AVS_LINK_TARGET "${AvisynthPlus_LIB_DIR}/avisynth.lib")
    elseif(WIN32)
        set(AVS_LINK_TARGET "${AvisynthPlus_LIB_DIR}/avisynth.dll")
    elseif(APPLE)
        set(AVS_LINK_TARGET "${AvisynthPlus_LIB_DIR}/libavisynth.dylib")
    else()
        set(AVS_LINK_TARGET "${AvisynthPlus_LIB_DIR}/libavisynth.so")
    endif()
    set_target_properties(AvsCore PROPERTIES
        INTERFACE_LINK_LIBRARIES "${AVS_LINK_TARGET}"
    )
  else()
    # If the target DOES exist (it's our placeholder), it is NOT imported.
    target_include_directories(AvsCore INTERFACE "${AvisynthPlus_INCLUDE_DIR}")
  endif()
endif()

# Mark the internal variable as advanced
mark_as_advanced(AvisynthPlus_INCLUDE_DIR)
