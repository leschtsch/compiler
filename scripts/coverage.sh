#! /usr/bin/bash

mkdir -p coverage
lcov --capture --directory build --include 'src/**' --exclude 'src/language_data/**' \
	--output-file coverage/coverage.info \
	--ignore-errors mismatch
genhtml -o coverage coverage/coverage.info
