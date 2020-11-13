#!/bin/bash

set -x
set -e

BUILD_DIRECTORY=$(realpath build/)

lcov --directory . -b "${BUILD_DIRECTORY}" --capture --initial -o coverage.base &&
lcov --rc lcov_branch_coverage=1 --directory . -b "${BUILD_DIRECTORY}" --capture -o coverage.capture &&
lcov --directory . -b "${BUILD_DIRECTORY}" --add-tracefile coverage.base --add-tracefile coverage.capture -o coverage.total &&
lcov --directory . -b "${BUILD_DIRECTORY}" --remove coverage.total '*/unit-tests/*' -o coverage.total &&
echo "Generated 'coverage.total'." &&
genhtml coverage.total -o coverage

rm -f coverage.base coverage.capture
