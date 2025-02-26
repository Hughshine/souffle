#!/bin/bash

# Compiler configuration
COMPILER="/usr/bin/c++"
INCLUDES="-I/home/hugh/research/datalog/souffle/src/include -I/usr/include"
STD_FLAG="-std=c++17"
CXX_FLAGS="-fopenmp"
DEBUG_FLAGS="-g"
DEFINITIONS="-DUSE_NCURSES -DUSE_LIBZ -DUSE_SQLITE"
LINK_OPTIONS="-ldl /usr/lib/x86_64-linux-gnu/libsqlite3.so /usr/lib/x86_64-linux-gnu/libz.so /usr/lib/x86_64-linux-gnu/libncurses.so"
RPATH="-Wl,-rpath,/usr/lib/x86_64-linux-gnu"

# Compile command
$COMPILER $INCLUDES $STD_FLAG $CXX_FLAGS $DEBUG_FLAGS $DEFINITIONS \
    main.cpp DerivationGraph.cpp Rule.cpp $LINK_OPTIONS $RPATH -o test

rm -f *.o

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful"
    # Run the program
    ./test
else
    echo "Compilation failed"
    exit 1
fi

