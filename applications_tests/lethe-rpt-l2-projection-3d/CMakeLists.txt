SET(TEST_LIBRARIES lethe)
SET(TEST_TARGET lethe-rpt-l2-projection-3d)

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_LOWER_BUILD_TYPE)

file(COPY rpt_l2_projection_files/positions.detector DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/rpt_l2_projection.${CMAKE_LOWER_BUILD_TYPE}/mpirun=1/)

DEAL_II_PICKUP_TESTS()


if (CMAKE_BUILD_TYPE STREQUAL "Debug" )
       	set_tests_properties(lethe-rpt-l2-projection-3d/rpt_l2_projection.mpirun=1.debug PROPERTIES TIMEOUT 1800)
ENDIF()