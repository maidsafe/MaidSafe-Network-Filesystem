#==================================================================================================#
#                                                                                                  #
#  Copyright 2012 MaidSafe.net limited                                                             #
#                                                                                                  #
#  This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,        #
#  version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which    #
#  licence you accepted on initial access to the Software (the "Licences").                        #
#                                                                                                  #
#  By contributing code to the MaidSafe Software, or to this project generally, you agree to be    #
#  bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root        #
#  directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available   #
#  at: http://www.maidsafe.net/licenses                                                            #
#                                                                                                  #
#  Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed    #
#  under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF   #
#  ANY KIND, either express or implied.                                                            #
#                                                                                                  #
#  See the Licences for the specific language governing permissions and limitations relating to    #
#  use of the MaidSafe Software.                                                                   #
#                                                                                                  #
#==================================================================================================#


set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/standard_setup.cmake")
  cmake_minimum_required(VERSION 2.8)  # To suppress warning cluttering error message
  set(Msg "\nThis project can currently only be build as part of the MaidSafe super-project.  For")
  set(Msg "${Msg} full details, see https://github.com/maidsafe/MaidSafe/wiki/Build-Instructions\n")
  message(FATAL_ERROR "${Msg}")
endif()

project(nfs)

include(../../cmake_modules/standard_setup.cmake)


#==================================================================================================#
# Set up all files as GLOBs                                                                        #
#==================================================================================================#

set(NfsSourcesDir ${PROJECT_SOURCE_DIR}/src/maidsafe/nfs)
ms_glob_dir(NfsDetail ${NfsSourcesDir}/detail "Nfs Detail")
ms_glob_dir(NfsTests ${NfsSourcesDir}/tests Tests)

#==================================================================================================#
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
ms_add_static_library(maidsafe_nfs_detail ${NfsDetailAllFiles})
target_include_directories(maidsafe_nfs_detail PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_nfs_detail maidsafe_common)

if(INCLUDE_TESTS)
  ms_add_executable(test_nfs "Tests/NFS" ${NfsTestsAllFiles})
  target_link_libraries(test_nfs maidsafe_nfs_detail maidsafe_test)
  # TODO - Investigate why boost variant requires this warning to be disabled.
  target_compile_options(test_nfs PRIVATE $<$<AND:$<BOOL:${MSVC}>,$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>>:/wd4702>)

  target_include_directories(test_nfs PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()

ms_rename_outdated_built_exes()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(INCLUDE_TESTS)
  ms_add_default_tests()
  ms_add_gtests(test_nfs)
  ms_test_summary_output()
endif()


#==================================================================================================#
# Package                                                                                          #
#==================================================================================================#
install(TARGETS maidsafe_nfs_detail COMPONENT Development CONFIGURATIONS Debug Release ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ COMPONENT Development DESTINATION include)
install(FILES ${OutputFile} COMPONENT Development DESTINATION include/maidsafe/nfs)

if(INCLUDE_TESTS)
  install(TARGETS test_nfs COMPONENT Tests CONFIGURATIONS Debug RUNTIME DESTINATION bin/debug)
  install(TARGETS test_nfs COMPONENT Tests CONFIGURATIONS Release RUNTIME DESTINATION bin)
endif()
