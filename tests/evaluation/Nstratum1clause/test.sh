time_command() {
    local command="$*"
    local start_time
    local end_time
    local elapsed

    # Get start time in seconds.nanoseconds
    start_time=$(date +%s.%N)

    # Execute the command
    eval "$command"

    # Get end time in seconds.nanoseconds
    end_time=$(date +%s.%N)

    # Calculate elapsed time
    elapsed=$(echo "$end_time - $start_time" | bc)
    printf "Command took: %.3f seconds\n" "$elapsed"
}

# temporary test script
sh ./clean.sh

## set up input and output folders
mkdir input
mkdir output
cp data/* input
bash gen.sh

## 1. build full compilation version
echo "Building full compilation" &
../../../cmake-build-debug/src/souffle test.dl -F./input -D./output -o test-full

## 2. build incremental compilation version
echo "Building incremental compilation" &
../../../cmake-build-debug/src/souffle test.dl --inc -F./input -D./output -o test-incr

## 3. build full with delta compilation version
echo "Building full-with-delta compilation" &
../../../cmake-build-debug/src/souffle test.dl --full-with-delta -F./input -D./output -o test-full-with-delta

## 3. run full and check its result
echo "Running full compilation" &
time_command "./test-full"

# 4. run incr and check its result
echo "Running incr compilation" &
time_command  "./test-incr"

# 6. run full-with-delta and check its result
echo "Running full-with-delta compilation" &
time_command  "./test-full-with-delta"

