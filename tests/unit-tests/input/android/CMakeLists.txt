list(APPEND UNIT_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test_android_input_lexicon.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_android_communication_package.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_android_input_sender.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_input_consumer.cpp
)

set(
  UNIT_TEST_SOURCES
  ${UNIT_TEST_SOURCES}
  PARENT_SCOPE)
