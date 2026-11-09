cmake_minimum_required(VERSION 3.13)
project(faac_cmake_consumer C)

# FAAC_CONSUMER_COMPONENTS selects what to link; empty links every built component.
set(FAAC_CONSUMER_COMPONENTS "" CACHE STRING "Components to request from the faac package")

find_package(faac CONFIG REQUIRED COMPONENTS ${FAAC_CONSUMER_COMPONENTS})

add_executable(consumer consumer.c)
foreach(c IN ITEMS faac faad3 faam faar)
  if(TARGET faac::${c})
    string(TOUPPER ${c} upper)
    target_compile_definitions(consumer PRIVATE USE_${upper}=1)
    target_link_libraries(consumer PRIVATE faac::${c})
    get_target_property(location faac::${c} IMPORTED_LOCATION)
    message(STATUS "faac::${c} -> ${location}")
  endif()
endforeach()

# test_link_check.c takes the address of every exported function, so linking it
# fails if the installed library lacks an object the header promises. consumer.c
# alone only pulls in what one call needs and would miss a gap in a static archive.
foreach(c IN ITEMS faac faad3 faam faar)
  if(TARGET faac::${c})
    string(REGEX REPLACE "3$" "" check ${c})
    string(TOUPPER ${check} check)
    add_executable(link_check_${c} ../test_link_check.c)
    target_compile_definitions(link_check_${c} PRIVATE CHECK_${check}=1)
    target_link_libraries(link_check_${c} PRIVATE faac::${c})
  endif()
endforeach()
