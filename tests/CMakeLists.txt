function(AddTestExec target)
	add_executable(${target} "./${target}.cpp")
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/exe"
	)
	if(ARGC GREATER 1)
		message("not link ps for target: ${target}")
	else()
		target_link_libraries(${target} ps_lib)
	endif()
endfunction()

AddTestExec(LR_normal)
AddTestExec(LR_ps)

# AddTestExec(test_connection)
# AddTestExec(test_simple_app)
# AddTestExec(test_kv_app)

# AddTestExec(test_kv_app_multi_workers)
# AddTestExec(test_kv_app_benchmark)

# AddTestExec(test_my)