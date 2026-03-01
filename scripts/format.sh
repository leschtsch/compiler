#! /usr/bin/bash

FILES="$(find src/ -name *.c -o -name *.h)"

for file in ${FILES}; do
	clang-format ${file} | diff ${file} -

	if [ $? -ne 0 ]; then
		echo check failed on ${file}
		exit
	fi
done

echo clang-format checks passed
