#!/bin/bash
# scripts/check-no-nested-configure.sh — repo hygiene check
# Detects nested CMake configure artifacts that clobber the outer
# CTestTestfile, causing ctest from the project root to report
# "No tests were found!!!" even though tests are registered.
#
# Per finalization/tasks.md T0.1 + finalization/design.md D1.1.
# Triggered by ctest housekeeping_no_nested_configure (tests/CMakeLists.txt).
#
# Exit code: 0 if clean, 1+ if any nested CMake cache detected.

set -e
errors=0
for path in build/tests build/examples-tests; do
    if [ -f "$path/CMakeCache.txt" ]; then
        echo "ERROR: nested cmake configure detected: $path/CMakeCache.txt"
        echo "  → 'cmake -B <subdir>' from inside build/ clobbers the outer CTestTestfile."
        echo "  → Fix: rm -rf '$path' (CI/CD builds should always start from clean)."
        errors=$((errors+1))
    fi
done
exit $errors