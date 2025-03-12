# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

#find_package(Python3 REQUIRED)
enable_testing()
set_property(GLOBAL PROPERTY CTEST_PARALLEL_LEVEL 8)


add_subdirectory(non_recursion)
add_subdirectory(recursion)
add_subdirectory(cycle)
#add_subdirectory(from_problog)
#add_subdirectory(from_datalog)
#add_subdirectory(evaluation)
