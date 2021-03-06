mir_add_wrapped_executable(mir_demo_standalone_render_surfaces
  render_surfaces.cpp
  buffer_render_target.cpp
  image_renderer.cpp
)

target_link_libraries(mir_demo_standalone_render_surfaces
  mirserver
  exampleserverconfig
  ${Boost_LIBRARIES}
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -fno-strict-aliasing -Wextra")

add_library(eglapp STATIC
  eglapp.c
  client_helpers.cpp
)

add_library(exampleserverconfig STATIC
  server_example_basic_window_manager.cpp
  server_example_basic_window_manager.h
  server_example_canonical_window_manager.cpp
  server_example_display_configuration_policy.cpp
  server_example_input_device_config.cpp
  server_example_input_event_filter.cpp
  server_example_log_options.cpp
  server_example_input_filter.cpp
  server_example_host_lifecycle_event.cpp
  server_example_window_management.cpp
  server_example_window_management_info.cpp
  server_example_window_management_info.h
  server_example_custom_compositor.cpp
  server_example_adorning_compositor.cpp
  server_example_cursor_images.cpp
  server_example_cursor_images.h
  xcursor_loader.cpp
  xcursor_loader.h
  xcursor.c
  xcursor.h
)

target_link_libraries(eglapp
  mirclient
  ${EGL_LIBRARIES}
  ${GLESv2_LIBRARIES}
  )

mir_add_wrapped_executable(mir_demo_client_animated_cursor
  animated_cursor_demo_client.c
)
target_link_libraries(mir_demo_client_animated_cursor
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_eglflash
  eglflash.c
)
target_link_libraries(mir_demo_client_eglflash
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_egltriangle
  egltriangle.c
)
target_link_libraries(mir_demo_client_egltriangle
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_target
  target.c
)
target_link_libraries(mir_demo_client_target
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_pointer_confinement
  pointer_confinement.c
)
target_link_libraries(mir_demo_client_pointer_confinement
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_camera
  camera.c
)
target_link_libraries(mir_demo_client_camera
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_eglcounter
  eglcounter.cpp
)
target_link_libraries(mir_demo_client_eglcounter
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_eglplasma
  eglplasma.c
)
target_link_libraries(mir_demo_client_eglplasma
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_cursors
  cursors_demo_client.c
)
target_link_libraries(mir_demo_client_cursors
  eglapp
)
mir_add_wrapped_executable(mir_demo_client_eglstateswitcher
  eglstateswitcher.c
)
target_link_libraries(mir_demo_client_eglstateswitcher
  eglapp
)

mir_add_wrapped_executable(mir_demo_client_tooltip
  tooltip.c
)
target_link_libraries(mir_demo_client_tooltip
  eglapp
  )

mir_add_wrapped_executable(mir_demo_client_touch_validator
  client_touch_validator.cpp
)
target_link_libraries(mir_demo_client_touch_validator
  eglapp
)

mir_add_wrapped_executable(mir_demo_client_basic
  basic.c
  )

mir_add_wrapped_executable(mir_demo_client_input_shapes
  input_shapes.cpp
  )
target_link_libraries(mir_demo_client_input_shapes eglapp)

target_link_libraries(mir_demo_client_basic
  mirclient

  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

mir_add_wrapped_executable(mir_demo_client_release_at_exit release_at_exit.c)
target_link_libraries(mir_demo_client_release_at_exit mirclient)

mir_add_wrapped_executable(mir_demo_client_multiwin multiwin.c)
target_link_libraries(mir_demo_client_multiwin mirclient)

mir_add_wrapped_executable(mir_demo_client_fingerpaint fingerpaint.c)
target_link_libraries(mir_demo_client_fingerpaint mirclient)

mir_add_wrapped_executable(mir_demo_client_progressbar progressbar.c)
target_link_libraries(mir_demo_client_progressbar mirclient)

mir_add_wrapped_executable(mir_demo_client_display_config demo_client_display_config.c)
target_link_libraries(mir_demo_client_display_config eglapp)

mir_add_wrapped_executable(mir_demo_client_eglsquare eglsquare.cpp)
target_link_libraries(mir_demo_client_eglsquare eglapp)

mir_add_wrapped_executable(mir_demo_client_flicker
  flicker.c
)

target_link_libraries(mir_demo_client_flicker
  mirclient

  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

mir_add_wrapped_executable(mir_demo_client_scroll
  scroll.cpp
)

target_link_libraries(mir_demo_client_scroll
  eglapp
  mirdraw
  ${GLESv2_LIBRARIES}
)

mir_add_wrapped_executable(mir_demo_client_prompt_session prompt_session.c)
target_link_libraries(mir_demo_client_prompt_session mirclient)

add_library(mirdraw STATIC graphics_utils.cpp)

include_directories(
  ${PROJECT_SOURCE_DIR}/include/server
  ${PROJECT_SOURCE_DIR}/include/client
  ${PROJECT_SOURCE_DIR}/include/platform
  ${PROJECT_SOURCE_DIR}/include/renderers/gl
  ${PROJECT_SOURCE_DIR}/include/renderers/sw
  ${GL_INCLUDE_DIRS}
)

mir_add_wrapped_executable(mir_demo_standalone_render_to_fb
    render_to_fb.cpp
)

target_link_libraries(mir_demo_standalone_render_to_fb
    mirserver
    mirdraw
    ${GL_LIBRARIES}
    ${Boost_LIBRARIES}
)

add_library(mir_demo_server_loadable MODULE
  server_example.cpp
  glog_logger.cpp
  server_example_test_client.cpp
)

target_link_libraries(mir_demo_server_loadable
  mirserver
  exampleserverconfig
  ${GLog_LIBRARY}
  ${GFlags_LIBRARY}
  ${Boost_LIBRARIES}
)

install(TARGETS mir_demo_server_loadable
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

mir_add_wrapped_executable(mir_demo_server
  mir_demo_server_loader.cpp
)

target_link_libraries(mir_demo_server
  dl
)

mir_add_wrapped_executable(mir_demo_server_minimal server_minimal.cpp)
target_link_libraries(mir_demo_server_minimal mirserver)

if (MIR_ENABLE_TESTS)
  add_executable(mir_symbol_test
      uses_non_client_symbol.cpp
  )
  target_link_libraries(mir_symbol_test
      mirclient
  )

  # Regression tests for LP: #1415321
  # To see reasons for failure:  ctest -V -R Clients-only-use-client-symbols
  mir_add_test(NAME Clients-only-use-client-symbols
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMAND /bin/sh -c "!(objdump -T mir_demo_client* | grep *UND* | grep mir | grep -v MIR_CLIENT)"
  )

  # Test our testcase...
  mir_add_test(NAME Client-using-symbols-from-mircommon-fails-check
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMAND /bin/sh -c "(objdump -T mir_symbol_test | grep *UND* | grep mir | grep -v MIR_CLIENT)"
  )

  # We can't use this test with clang as that generates spurious NEEDED
  if (NOT "${CMAKE_CXX_COMPILER}" MATCHES "clang")
    # To see reasons for failure:  ctest -V -R Clients-only-use-client-ABI
    mir_add_test(NAME Clients-only-use-client-ABI
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
      COMMAND /bin/sh -c "!(objdump -p mir_demo_client*  | grep NEEDED | grep libmir | grep -v libmirclient)"
    )
    # Again, test the testcase...
    mir_add_test(NAME Client-using-ABI-from-mircommon-fails-check
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
      COMMAND /bin/sh -c "(objdump -p mir_symbol_test | grep NEEDED | grep libmir | grep -v libmirclient)"
    )
  endif ()
endif ()

add_executable(mir_cursor_theme_dump
  cursor-theme-dump.cpp
)

target_link_libraries(mir_cursor_theme_dump
  exampleserverconfig
  mirserver
)

mir_add_wrapped_executable(mir_demo_client_multistream
  multi_stream.cpp
)

target_link_libraries(mir_demo_client_multistream
  mirclient
  eglapp
  ${Boost_LIBRARIES}
)
