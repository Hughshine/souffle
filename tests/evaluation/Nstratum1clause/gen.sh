# Output file name
OUTPUT_FILE="input/I.facts"

# Number of lines to generate
NUM_LINES=100000

# Generate the pairs
for ((i=1; i<=NUM_LINES; i++)); do
    printf "%d\t%d\n" $i $((i+1)) >> "$OUTPUT_FILE"
done

echo "Generated $NUM_LINES lines in $OUTPUT_FILE"