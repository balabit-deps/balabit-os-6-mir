
# Warnings in 3rd party code are not to be considered errors. :(
string (REPLACE " -Werror " " " CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

list(
  APPEND MIR_3RD_PARTY_INCLUDE_DIRECTORIES
  ${CMAKE_CURRENT_SOURCE_DIR}/android-deps
  )

set(MIR_3RD_PARTY_INCLUDE_DIRECTORIES ${MIR_3RD_PARTY_INCLUDE_DIRECTORIES} PARENT_SCOPE)

include_directories(${PROJECT_SOURCE_DIR}/src/include/cookie)
include_directories(${PROJECT_SOURCE_DIR}/src/include/common)
include_directories(${PROJECT_SOURCE_DIR}/src/include/platform)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/android-deps)
add_subdirectory(android-input)

set(MIR_ANDROID_INCLUDE_DIRECTORIES ${MIR_ANDROID_INCLUDE_DIRECTORIES} PARENT_SCOPE)
set(
  MIR_ANDROID_INPUT_COMPILE_FLAGS
  ${MIR_ANDROID_INPUT_COMPILE_FLAGS}
  PARENT_SCOPE)
