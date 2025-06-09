#!/bin/bash

# CUDD configuration
CUDD_ROOT="/usr/local"
CUDD_INCLUDE="${CUDD_ROOT}/include"
CUDD_LIB="${CUDD_ROOT}/lib"
CUDD_STATIC_LIB="${CUDD_LIB}/libcudd.a"  # Full path to the static library

# Compiler configuration
COMPILER="/usr/bin/c++"
INCLUDES="-I/home/hugh/research/datalog/souffle/src/include -I/usr/include -I${CUDD_INCLUDE}"
STD_FLAG="-std=c++17"
CXX_FLAGS="-fopenmp"
DEBUG_FLAGS="-g"
DEFINITIONS="-DUSE_NCURSES -DUSE_LIBZ -DUSE_SQLITE"

# When using static libraries, order matters significantly
# The object file comes first, then the libraries it depends on
LINK_OPTIONS="-ldl /usr/lib/x86_64-linux-gnu/libsqlite3.so /usr/lib/x86_64-linux-gnu/libz.so /usr/lib/x86_64-linux-gnu/libncurses.so"
RPATH="-Wl,-rpath,/usr/lib/x86_64-linux-gnu"

# Compile and link command
echo "Compiling with CUDD static library: ${CUDD_STATIC_LIB}"
$COMPILER $INCLUDES $STD_FLAG $CXX_FLAGS $DEBUG_FLAGS $DEFINITIONS \
    main.cpp ${CUDD_STATIC_LIB} $LINK_OPTIONS $RPATH -o test

# Check if compilation was successful
STATUS=$?
if [ $STATUS -eq 0 ]; then
    echo "Compilation successful"
    rm -f *.o
    # Run the program
    ./test
else
    echo "Compilation failed with status $STATUS"
    exit 1
fi