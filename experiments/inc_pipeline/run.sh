#!/bin/bash
python ../../cmake-build-debug/src/souffle-compile.py main.cpp -o test --with-cudd -v

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

