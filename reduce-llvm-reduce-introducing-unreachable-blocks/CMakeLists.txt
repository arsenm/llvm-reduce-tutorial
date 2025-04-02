file(GENERATE OUTPUT interestingness.sh
     INPUT interestingness.sh.in
     FILE_PERMISSIONS ${script_permissions})

set(YO_DAWG_INTERESTINGNESS ${CMAKE_CURRENT_BINARY_DIR}/interestingness.sh)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/yo_dawg.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/yo_dawg.sh.tmp @ONLY)
file(GENERATE OUTPUT yo_dawg.sh
     INPUT ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/yo_dawg.sh.tmp
     FILE_PERMISSIONS ${script_permissions})

set(YO_DAWG ${CMAKE_CURRENT_BINARY_DIR}/yo_dawg.sh)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/meta-reducer.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/meta-reducer.sh.tmp @ONLY)

file(GENERATE OUTPUT meta-reducer.sh
     INPUT ${CMAKE_CURRENT_BINARY_DIR}/build_temporaries/meta-reducer.sh.tmp
     FILE_PERMISSIONS ${script_permissions})
