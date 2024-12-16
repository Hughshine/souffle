# temporary test script
sh ./clean.sh

## set up input and output folders
mkdir input
mkdir output
cp data/* input

## 1. build full compilation version
echo "Building full compilation" &
../../../cmake-build-debug/src/souffle test.dl -F./input -D./output -o test-full

## 2. build incremental compilation version
echo "Building incremental compilation" &
../../../cmake-build-debug/src/souffle test.dl --inc -F./input -D./output -o test-incr

### 3. run full and check its result
echo "Running full compilation" &
./test-full
if cmp -s test.dl.json result/test.dl.json.full; then
  echo "SUCCESS: Full compilation" &
else
  echo "FAIL: Full compilation" &
fi

# 4. run incr and check its result
echo "Running incr compilation" &
./test-incr
if cmp -s test.dl.json result/test.dl.json.incr; then
  echo "SUCCESS: incremental compilation, derivations" &
else
  echo "FAIL: incremental compilation, derivations" &
fi

if cmp -s test.dl.insert.json result/test.dl.insert.json.incr; then
  echo "SUCCESS: incremental compilation, inserted derivations" &
else
  echo "FAIL: incremental compilation, inserted derivations" &
fi

if cmp -s test.dl.delete.json result/test.dl.delete.json.incr; then
  echo "SUCCESS: incremental compilation, deleted derivations" &
else
  echo "FAIL: incremental compilation, deleted derivations" &
fi