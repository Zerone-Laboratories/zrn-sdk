# one libFuzzer executable per file; the library is built with the same sanitizers
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR "ZRNUI_BUILD_FUZZERS needs clang")
endif()

# the fuzzers call internals (zrn_*), which a shared zrnui hides: they link the static twin when there is one
foreach(library zrnui ${ZRNUI_TEST_LIBRARY})
	target_compile_options(${library} PRIVATE -fsanitize=fuzzer-no-link,address,undefined -g)
	target_link_options(${library} PUBLIC -fsanitize=address,undefined)
endforeach()

foreach(name fuzz_text fuzz_png fuzz_theme)
	add_executable(${name} ${name}.c)
	target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined -g)
	target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
	target_compile_definitions(${name} PRIVATE _POSIX_C_SOURCE=200809L)
	target_link_libraries(${name} PRIVATE ${ZRNUI_TEST_LIBRARY})
endforeach()
