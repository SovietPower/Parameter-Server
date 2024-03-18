function(AddTestExec target)
	add_executable(${target} "./${target}.cpp")
	target_link_libraries(${target} ps_lib)
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/exe"
	)
endfunction()

AddTestExec(test_connection)
AddTestExec(test_simple_app)
AddTestExec(test_kv_app)

AddTestExec(test_kv_app_multi_workers)
AddTestExec(test_kv_app_benchmark)

AddTestExec(test_my)