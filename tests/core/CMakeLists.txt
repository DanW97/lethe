set(TEST_LIBRARIES lethe-core)

string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type)
file(COPY table_read_input.dat DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/table_read.${_build_type}")
file(COPY hex_extrusion_cad.step DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shape_opencascade.${_build_type}/mpirun=1")

deal_ii_pickup_tests()
