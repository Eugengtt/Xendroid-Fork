Add-Type -AssemblyName System.IO.Compression.FileSystem
$paths = @(
'C:\Users\meuge\.gradle\caches\modules-2\files-2.1\androidx.compose.ui\ui-android\1.7.3\736552c288e59464002f85c1e50d8190f38fdc0f\ui-release.aar',
'C:\Users\meuge\.gradle\caches\modules-2\files-2.1\androidx.compose.foundation\foundation-android\1.7.3\2d32f84d52939645470069c44e46aa24b1bbb0f6\foundation-release.aar',
'C:\Users\meuge\.gradle\caches\modules-2\files-2.1\androidx.compose.ui\ui-graphics-android\1.7.3\ae11ca5399e831a528a181c729a1e7f3cd78910c\ui-graphics-release.aar'
)
$tmpJar = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), 'classes-scope.jar')
foreach ($p in $paths) {
  "=== $p ==="
  $aarszip = [System.IO.Compression.ZipFile]::OpenRead($p)
  $jarEntry = $aarszip.Entries | Where-Object { $_.FullName -eq 'classes.jar' }
  if ($jarEntry) {
    $stream = [System.IO.File]::OpenWrite($tmpJar)
    $jarEntry.Open().CopyTo($stream)
    $stream.Dispose()
    $jarzip = [System.IO.Compression.ZipFile]::OpenRead($tmpJar)
    $jarzip.Entries | Where-Object { $_.FullName -like '*draw*' -or $_.FullName -like '*Draw*' } | Select-Object -Expand FullName
    $jarzip.Dispose()
  }
  $aarszip.Dispose()
}
[System.IO.File]::Delete($tmpJar)
