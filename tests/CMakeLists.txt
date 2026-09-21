# The tests reach into the private headers of src/ and link the hidden symbols, which
# a shared library does not export; with BUILD_SHARED_LIBS they link a static copy.
if(BUILD_SHARED_LIBS)
    add_library(hh_testlib STATIC)
    foreach(source IN LISTS HH_SOURCES)
        target_sources(hh_testlib PRIVATE ${PROJECT_SOURCE_DIR}/${source})
    endforeach()
    target_compile_features(hh_testlib PUBLIC cxx_std_17)
    target_include_directories(hh_testlib PUBLIC ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
    set_target_properties(hh_testlib PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    set(HH_TEST_LIBRARY hh_testlib)
else()
    set(HH_TEST_LIBRARY hh)
endif()

# The C ABI is exercised from a C translation unit.
enable_language(C)

add_executable(hh_tests
    test_main.cpp
    test_c_api.c
    test_c_api_runner.cpp
    test_contrast.cpp
    test_derive.cpp
    test_encode.cpp
    test_features.cpp
    test_fuzz.cpp
    test_hmac.cpp
    test_pbkdf2.cpp
    test_raster.cpp
    test_sha256.cpp
    test_statistics.cpp
    test_vectors.cpp
)
target_link_libraries(hh_tests PRIVATE ${HH_TEST_LIBRARY})
target_include_directories(hh_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(hh_tests PRIVATE HH_TESTDATA_DIR="${PROJECT_SOURCE_DIR}/testdata")
set_target_properties(hh_tests PROPERTIES CXX_EXTENSIONS OFF C_STANDARD 99 C_EXTENSIONS OFF)
hh_set_warnings(hh_tests)

add_test(NAME hh_tests COMMAND hh_tests)

# The generator of the golden vectors; its check mode proves that testdata/ is
# what the reference implementation produces.
add_executable(gen_vectors ${PROJECT_SOURCE_DIR}/tools/gen_vectors.cpp)
target_link_libraries(gen_vectors PRIVATE ${HH_TEST_LIBRARY})
target_include_directories(gen_vectors PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/tools)
set_target_properties(gen_vectors PROPERTIES CXX_EXTENSIONS OFF)
hh_set_warnings(gen_vectors)

add_test(NAME golden_vectors_match COMMAND gen_vectors --check ${PROJECT_SOURCE_DIR}/testdata)
