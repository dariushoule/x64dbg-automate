Set-Location $PSScriptRoot
& .\build-install-32.cmd; if ($LASTEXITCODE -ne 0) { throw "32-bit build failed" }
& .\build-install-64.cmd; if ($LASTEXITCODE -ne 0) { throw "64-bit build failed" }

$version = "0.8.1-ghost_fungus"

if (Test-Path "release32-$version.zip") {
    Remove-Item "release32-$version.zip"
}

if (Test-Path "release64-$version.zip") {
    Remove-Item "release64-$version.zip"
}

Compress-Archive -Path "build32\Release" -Update -DestinationPath "release32-$version.zip"
Compress-Archive -Path "build64\Release" -Update -DestinationPath "release64-$version.zip"