param(
    [string]$BuildDir = "build"
)

cmake -S . -B $BuildDir -G "Ninja"
