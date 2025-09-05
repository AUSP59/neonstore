$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$bin = Join-Path $toolsDir "neonstore.exe"
Install-BinFile -Name "neonstore" -Path $bin
