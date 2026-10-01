#!/bin/sh
# Runs the unit tests, then the demo. Pass "tests", "benchmark" or "demo" to run only one.
# Extra arguments go to acalloc, e.g.:  docker run --rm image demo --lang en
set -e
echo "=== adaptive_capacity_allocator, compiler: ${COMPILER} ==="
cmd="$1"; [ $# -gt 0 ] && shift
case "$cmd" in
  tests)     exec ./adaptive_tests ;;
  benchmark) exec ./adaptive_benchmark "$@" ;;
  demo)      exec ./acalloc demo "$@" ;;
  *)         ./adaptive_tests && ./acalloc demo ;;
esac
