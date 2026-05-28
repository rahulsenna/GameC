#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "Usage: ./utilities/run.sh <script_name_without_extension>"
  echo "Example: ./utilities/run.sh inspect_fbx"
  exit 1
fi

SCRIPT_NAME=$1

# Ensure we are in the project root (where this script is usually called from)
# or cd to it if called from within utilities directory.
cd "$(dirname "$0")/.."

mkdir -p utilities/build

echo "Compiling $SCRIPT_NAME..."
clang++ -std=c++14 utilities/$SCRIPT_NAME.cpp -o utilities/build/$SCRIPT_NAME

echo "--- Running $SCRIPT_NAME ---"
./utilities/build/$SCRIPT_NAME
