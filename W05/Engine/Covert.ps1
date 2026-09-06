$exts = @("*.h","*.hpp","*.c","*.cpp","*.inl")
$utf8bom = New-Object System.Text.UTF8Encoding($true)
$enc949 = [System.Text.Encoding]::GetEncoding(949)

Get-ChildItem -Path ..\Engine\Source -Recurse -Include $exts | ForEach-Object {
    $path = $_.FullName
    $bytes = [System.IO.File]::ReadAllBytes($path)

    # 1. BOM 체크 (이미 UTF-8 BOM이면 건너뜀)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        Write-Host "Skip (UTF-8 BOM): $path"
        return
    }

    # 2. UTF-8로 읽기 시도 (엄격하게)
    try {
        $utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
        $text = $utf8Strict.GetString($bytes)

        # 문제 없이 읽히면 UTF-8이라 판단 → 건너뜀
        Write-Host "Skip (Valid UTF-8): $path"
    }
    catch {
        # 3. UTF-8 실패 → CP949로 간주 후 변환
        $text = $enc949.GetString($bytes)
        [System.IO.File]::WriteAllText($path, $text, $utf8bom)
        Write-Host "Converted (CP949 → UTF-8 BOM): $path"
    }
}