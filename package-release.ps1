.\build-install-32.cmd
.\build-install-64.cmd

$version = "0.5.0-pingpong_bat"

if (Test-Path "release32-$version.zip") {
    Remove-Item "release32-$version.zip"
}

if (Test-Path "release64-$version.zip") {
    Remove-Item "release64-$version.zip"
}

Compress-Archive -Path "build32\Release" -Update -DestinationPath "release32-$version.zip"
Compress-Archive -Path "build64\Release" -Update -DestinationPath "release64-$version.zip"