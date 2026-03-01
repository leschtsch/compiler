#! /usr/bin/bash

echo -e "\033[1mbuilding: \033[0m"

BUILD_TYPES="Debug CodeCoverage Release"

for build_type in ${BUILD_TYPES}; do
	mkdir -p build/${build_type}
	pushd build/${build_type}
	cmake -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTS=YES ../..
	make
	popd
done

touch build/compile_commands.json
jq -s add $(find -wholename "./build/*/**.json") >build/compile_commands.json

echo -e "\033[1mcode checks: \033[0m"
source scripts/format.sh
source scripts/tidy.sh

echo -e "\033[1munit testing: \033[0m"

for build_type in ${BUILD_TYPES}; do
	pushd build/${build_type}
	ctest -j4 --output-on-failure
	popd
done

./scripts/coverage.sh
