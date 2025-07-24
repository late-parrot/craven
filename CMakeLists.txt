cmake_minimum_required(VERSION 3.12)

project(raven VERSION 0.0.0)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file(src/config.h.in ../src/config.h)

file(GLOB raven_SRC CONFIGURE_DEPENDS "src/*.h" "src/*.c")

add_executable(raven ${raven_SRC})

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(raven PUBLIC ${MATH_LIBRARY})
endif()

install(TARGETS raven DESTINATION bin)

enable_testing()

# define a function to simplify adding tests
function(do_test test_name arg result)
  add_test(NAME ${test_name} COMMAND raven ${arg})
  set_tests_properties(${test_name}
    PROPERTIES PASS_REGULAR_EXPRESSION ${result}
  )
endfunction(do_test)

do_test(anonFunc "../test/unittest/anonFunc.rvn" "<func anonymous>\n5")