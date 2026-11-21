# one executable per file
set(ZRNUI_TESTS test_layout test_controls test_input test_render test_navigation test_gpu test_animation test_pivot test_items test_bars test_flyouts test_hub test_pickers test_richtext test_resources test_fonts test_config test_gaps)

foreach(name ${ZRNUI_TESTS})
	add_executable(${name} ${name}.c)
	target_link_libraries(${name} PRIVATE ${ZRNUI_TEST_LIBRARY} PkgConfig::PNG m)
	target_compile_definitions(${name} PRIVATE ZRNUI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
	if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unused-parameter)
	endif()
	add_test(NAME ${name} COMMAND ${name})
endforeach()

# test_gpu asks the GL context it drew with whether it is a GPU and can multisample; it exits 77 when it cannot run here
if(GLES_FOUND)
	target_link_libraries(test_gpu PRIVATE PkgConfig::GLES)
	target_compile_definitions(test_gpu PRIVATE ZRNUI_TEST_HAVE_GLES)
endif()
set_tests_properties(test_gpu PROPERTIES SKIP_RETURN_CODE 77)

# the seam to the platform layer, used the way a shell does: with an EGL context of its own
if(GLES_FOUND)
	add_executable(test_platform test_platform.c)
	target_link_libraries(test_platform PRIVATE ${ZRNUI_TEST_LIBRARY} PkgConfig::PNG PkgConfig::GLES m)
	target_compile_definitions(test_platform PRIVATE ZRNUI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
	add_test(NAME test_platform COMMAND test_platform)
	set_tests_properties(test_platform PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

# test_resources reads the internal color table to check that it agrees with the generated one
target_link_libraries(test_fonts PRIVATE PkgConfig::FREETYPE)
target_link_libraries(test_gaps PRIVATE PkgConfig::FREETYPE)

# the EGL driver leaks a little of its own at exit, which LeakSanitizer would report as ours
set_tests_properties(test_gpu test_animation test_gaps PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
