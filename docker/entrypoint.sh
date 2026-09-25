#!/bin/sh
# Runs the unit tests, then the benchmark (the algorithm demo).
# Pass "tests" or "benchmark" to run only one of them.
set -e
echo "=== adaptive_capacity_allocator, compiler: ${COMPILER} ==="
case "$1" in
  tests)     exec ./adaptive_tests ;;
  benchmark) exec ./adaptive_benchmark ;;
  *)         ./adaptive_tests && ./adaptive_benchmark ;;
esac
