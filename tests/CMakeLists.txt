
add_executable(unit_generative test_generative.cpp)
target_link_libraries(unit_generative PRIVATE neonstore_lib)
add_test(NAME unit_generative COMMAND unit_generative)

add_test(NAME e2e_cli_print COMMAND $<TARGET_FILE:neonstore> analytics summary --dir ${CMAKE_SOURCE_DIR}/examples/minidataset --output json)

add_executable(unit_sqlite test_sqlite.cpp)
target_link_libraries(unit_sqlite PRIVATE neonstore_lib)
add_test(NAME unit_sqlite COMMAND unit_sqlite)

add_executable(unit_csv_crlf test_csv_crlf.cpp)
target_link_libraries(unit_csv_crlf PRIVATE neonstore_lib)
add_test(NAME unit_csv_crlf COMMAND unit_csv_crlf)
