.\build-install-32.cmd
.\build-install-64.cmd

if (Test-Path "release32-0.1.0-bitter_oyster.zip") {
    Remove-Item "release32-0.1.0-bitter_oyster.zip"
}

if (Test-Path "release64-0.1.0-bitter_oyster.zip") {
    Remove-Item "release64-0.1.0-bitter_oyster.zip"
}

Compress-Archive -Path "build32\Release" -Update -DestinationPath "release32-0.1.0-bitter_oyster.zip"
Compress-Archive -Path "build64\Release" -Update -DestinationPath "release64-0.1.0-bitter_oyster.zip"