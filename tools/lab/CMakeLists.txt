# The design lab. Research tooling only: floating point and threads are
# allowed here, nothing is installed, and the library never depends on it.

find_package(Threads REQUIRED)

add_executable(hh_lab
    lab_main.cpp
    candidate.cpp
    canvas.cpp
    colour.cpp
    contact_sheets.cpp
    directions.cpp
    grinding.cpp
    markers.cpp
    palette_gate.cpp
    png_writer.cpp
    prior_art.cpp
    selftest.cpp
    sheet.cpp
    stretch_bench.cpp
    surfaces.cpp
)
target_link_libraries(hh_lab PRIVATE hh Threads::Threads)
target_include_directories(hh_lab PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(hh_lab PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(hh_lab PRIVATE cxx_std_17)
hh_set_warnings(hh_lab)

# The benchmark reports the flags it was built with.
string(TOUPPER "${CMAKE_BUILD_TYPE}" HH_LAB_BUILD_TYPE_UPPER)
target_compile_definitions(hh_lab PRIVATE
    HH_LAB_BUILD_FLAGS="${CMAKE_BUILD_TYPE} ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${HH_LAB_BUILD_TYPE_UPPER}}")
