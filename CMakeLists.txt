cmake_minimum_required(VERSION 3.3.0)

include(CMakeParseArguments)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# add prefix to a list of strings
function(addprefix var prefix)
  foreach(arg ${ARGN})
    list(APPEND tmp "-I${arg}")
  endforeach()
  set(${var}
      ${tmp}
      PARENT_SCOPE)
endfunction()

# filter out flags that are not appropriate for the compiler being used
function(target_compile_options_filtered target visibility)
  foreach(flag ${ARGN})
    if(flag MATCHES "^-D.*")
      target_compile_options(${target} ${visibility} ${flag})
    else()
      check_c_compiler_flag(${flag} C_COMPILER_SUPPORTS__${flag})
      if(C_COMPILER_SUPPORTS__${flag})
        target_compile_options(${target} ${visibility}
                               $<$<COMPILE_LANGUAGE:C>:${flag}>)
      endif()

      check_cxx_compiler_flag(${flag} CXX_COMPILER_SUPPORTS__${flag})
      if(CXX_COMPILER_SUPPORTS__${flag})
        target_compile_options(${target} ${visibility}
                               $<$<COMPILE_LANGUAGE:CXX>:${flag}>)
      endif()
    endif()
  endforeach()
endfunction()

project(abc)

if(READLINE_FOUND MATCHES TRUE)
  addprefix(ABC_READLINE_INCLUDES_FLAGS "-I" ${READLINE_INCLUDE})
  string(REPLACE ";" " " ABC_READLINE_INCLUDES_FLAGS
                 "${ABC_READLINE_INCLUDES_FLAGS}")
  list(APPEND ABC_READLINE_FLAGS
       "ABC_READLINE_INCLUDES=${ABC_READLINE_INCLUDES_FLAGS}")

  string(REPLACE ";" " " ABC_READLINE_LIBRARIES_FLAGS "${READLINE_LIBRARIES}")
  list(APPEND ABC_READLINE_FLAGS
       "ABC_READLINE_LIBRARIES=${ABC_READLINE_LIBRARIES_FLAGS}")
elseif(READLINE_FOUND MATCHES FALSE)
  list(APPEND ABC_READLINE_FLAGS "ABC_USE_NO_READLINE=1")
endif()

if(ABC_USE_NAMESPACE)
  set_source_files_properties(${ABC_SRC} PROPERTIES LANGUAGE CXX)
endif()

if(ABC_USE_NAMESPACE)
  set(ABC_USE_NAMESPACE_FLAGS "ABC_USE_NAMESPACE=${ABC_USE_NAMESPACE}")
endif()

if(APPLE)
  set(MAKE_ENV ${CMAKE_COMMAND} -E env SDKROOT=${CMAKE_OSX_SYSROOT})
endif()

# run make to extract compiler options, linker options and list of source files
execute_process(
  COMMAND
    ${MAKE_ENV} make ${ABC_READLINE_FLAGS} ${ABC_USE_NAMESPACE_FLAGS}
    ARCHFLAGS_EXE=${CMAKE_CURRENT_BINARY_DIR}/abc_arch_flags_program.exe
    ABC_MAKE_NO_DEPS=1 CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
    LD=${CMAKE_CXX_COMPILER} cmake_info
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE MAKE_OUTPUT)

# extract options from make output
function(extract_var separator dest_variable make_output)
  string(REGEX MATCH "${separator} .* ${separator}" TMP "${make_output}")
  string(REGEX REPLACE "${separator} (.*) ${separator}" "\\1" TMP "${TMP}")

  separate_arguments(TMP UNIX_COMMAND "${TMP}")

  set(${dest_variable}
      ${TMP}
      PARENT_SCOPE)
endfunction()

extract_var(SEPARATOR_SRC ABC_SRC ${MAKE_OUTPUT})
extract_var(SEPARATOR_LIBS ABC_LIBS ${MAKE_OUTPUT})
extract_var(SEPARATOR_CFLAGS ABC_CFLAGS ${MAKE_OUTPUT})
extract_var(SEPARATOR_CXXFLAGS ABC_CXXFLAGS ${MAKE_OUTPUT})

set(ABC_MAIN_SRC src/base/main/main.c)
list(REMOVE_ITEM ABC_SRC ${ABC_MAIN_SRC})

add_library(libabc EXCLUDE_FROM_ALL ${ABC_SRC})

target_include_directories(libabc
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/abc)
target_include_directories(
  libabc INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/abc>
                   $<INSTALL_INTERFACE:include/abc>)

target_compile_definitions(libabc INTERFACE ABC_USE_STDINT_H)
target_compile_options_filtered(
  libabc
  PRIVATE
  ${ABC_CFLAGS}
  ${ABC_CXXFLAGS}
  -Wno-unused-but-set-variable
  -Wno-unused-variable
  -Wno-macro-redefined
  -Wno-deprecated-declarations
  -Wno-deprecated)
target_link_libraries(libabc PRIVATE ${ABC_LIBS})
set_property(TARGET libabc PROPERTY OUTPUT_NAME abc)

add_executable(abc ${ABC_MAIN_SRC})
target_link_libraries(abc PRIVATE libabc)
target_include_directories(abc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options_filtered(abc PRIVATE ${ABC_CFLAGS} ${ABC_CXXFLAGS}
                                -Wno-unused-but-set-variable)
target_link_libraries(abc PRIVATE ${ABC_LIBS})
