cmake_minimum_required(VERSION 3.1)
project(pedrokv)

add_subdirectory(deps/pedrodb)
add_subdirectory(deps/pedronet)
file(GLOB_RECURSE PEDROKV_SRCS src/*.cc)

add_library(pedrokv SHARED ${PEDROKV_SRCS} include/pedrokv/codec/client_codec.h)
target_compile_features(pedrokv PRIVATE cxx_std_17)
target_include_directories(pedrokv PUBLIC include)
target_link_libraries(pedrokv PRIVATE pedrodb pedrolib pedronet)

add_executable(pedrokv_test_server test/test_server.cc)
target_compile_features(pedrokv_test_server PRIVATE cxx_std_17)
target_link_libraries(pedrokv_test_server PRIVATE pedrodb pedrolib pedronet pedrokv)

add_executable(pedrokv_test_client test/test_client.cc)
target_compile_features(pedrokv_test_client PRIVATE cxx_std_17)
target_link_libraries(pedrokv_test_client PRIVATE pedrodb pedrolib pedronet pedrokv)

add_executable(pedrokv_test_request test/codec/test_request.cc)
target_compile_features(pedrokv_test_request PRIVATE cxx_std_17)
target_link_libraries(pedrokv_test_request PRIVATE pedrodb pedrolib pedronet pedrokv)