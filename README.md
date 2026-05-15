# Compiler
This is educational project -- compiler for some c-like language.

Currently it parses program from stdin and prints AST in graphviz `.dot` format to stdout.

## How to run
./runme.sh builds from scratch every time using different build profiles, runs test etc.

Or you may build it manually using cmake. You probably want
```bash
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE="Release"
    make -j4
```

To run tests:
```bash
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE="Release" -DENABLE_TESTS=YES
    make -j4
    ctest .
```

Other build types include `Debug`, `CodeCoverage`, `ClangBuildAnalyze` and `Perf`.
Any other build type (or no build type) will result in 
build with user defined `CMAKE_CXX_FLAGS` only.
