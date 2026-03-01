#! /usr/bin/bash

mkdir -p coverage
lcov --capture --directory build --include 'src/**' \
	--output-file coverage/coverage.info \
	--ignore-errors mismatch
genhtml -o coverage coverage/coverage.info
