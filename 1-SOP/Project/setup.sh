#!/bin/bash

# Setup directories
SRC="test"
TARGET="test_target_1"

# Clean up previous runs
rm -rf "$SRC" "$TARGET"
mkdir -p "$SRC"

# Regular Files & Nested Directories
echo "Root file content" > "$SRC/root_file.txt"
mkdir -p "$SRC/subdir/nested"
echo "Nested file content" > "$SRC/subdir/nested/deep_file.txt"

# Filenames with spaces
echo "Spaces content" > "$SRC/file with spaces.txt"
mkdir -p "$SRC/dir with spaces"
echo "Inner content" > "$SRC/dir with spaces/inner.txt"

# Symbolic Links

# Relative link (should be copied unchanged)
ln -s "root_file.txt" "$SRC/link_relative.txt"

# Absolute link pointing OUTSIDE source (should be copied unchanged)
ln -s "/etc/hosts" "$SRC/link_external_abs"

# Absolute link pointing INSIDE source
# The backup must detect this is internal and adjust the target link.
ABS_PATH=$(realpath "$SRC")
ln -s "$ABS_PATH/subdir/nested/deep_file.txt" "$SRC/link_internal_abs"

echo "--- Test Environment Created ---"
echo "Source Directory: $SRC"
ls -lR "$SRC"