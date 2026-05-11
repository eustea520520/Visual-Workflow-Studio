param(
    [string]$BuildDir = "build"
)

cmake --build $BuildDir
ctest --test-dir $BuildDir --output-on-failure
