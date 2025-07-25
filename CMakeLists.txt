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
do_test(arithmetic "../test/unittest/arithmetic.rvn" "2\n4\\.25\nMath!")
do_test(closures "../test/unittest/closures.rvn" "1\n2\n3")
do_test(dicts "../test/unittest/dicts.rvn" "2\ndict \\{1 => 2, 2 => 3, 3 => 4\\}")
do_test(for "../test/unittest/for.rvn" "1\n4\n9\n16\n25\n16\n9\n4\n1")
do_test(lists "../test/unittest/lists.rvn" "w\no\nw\n5\n\\[1, 2, 3\\]\n1\n2\n3\n\\[12, 2, 3, 4\\]")
do_test(options "../test/unittest/options.rvn" "1\n6\n5")
do_test(full_test "../test/unittest/test.rvn" "Hello, world!\n3\\.6288e\\+06\n5\n5\ntrue\nfalse\n0\n1\n1\n1\n0\n0\n0\n1\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n10\n8")
do_test(vec "../test/unittest/vec.rvn" "3\n5")