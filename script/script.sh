# store the first cli arg, with a default value of 0
NUM_THREADS="${1:-0}"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
RES_DIR="$DIR/res"

mkdir -p "$RES_DIR"
cp "$DIR/correct_output.txt" "$RES_DIR/correct_output.txt"

sudo "$DIR/../build/bin/ft_nmap" $NUM_THREADS > "$RES_DIR/1output.txt"
sudo "$DIR/../build/bin/ft_nmap" $NUM_THREADS > "$RES_DIR/2output.txt"
sudo "$DIR/../build/bin/ft_nmap" $NUM_THREADS > "$RES_DIR/3output.txt"

#check if there are diff between the outputs
diff "$RES_DIR/1output.txt" "$RES_DIR/correct_output.txt" > "$RES_DIR/diff1"
diff "$RES_DIR/2output.txt" "$RES_DIR/3output.txt" > "$RES_DIR/diff2"
diff "$RES_DIR/1output.txt" "$RES_DIR/2output.txt" > "$RES_DIR/diff3"

#check if the diffs are empty
if [ -s "$RES_DIR/diff1" ] || [ -s "$RES_DIR/diff2" ] || [ -s "$RES_DIR/diff3" ]; then
    echo "ERROR: The outputs are not the same"
    echo "Diff 1:"
    cat "$RES_DIR/diff1"
    echo "Diff 2:"
    cat "$RES_DIR/diff2"
    echo "Diff 3:"
    cat "$RES_DIR/diff3"
else
    echo "GOOD: All the outputs are the same"
fi

#rm -rf res