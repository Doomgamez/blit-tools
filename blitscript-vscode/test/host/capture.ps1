# saves a picture of the test's VS Code window, so the webviews can be checked by eye
param($titleLike, $output)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CaptureWindow {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
}
'@

$process = Get-Process Code -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like $titleLike } | Select-Object -First 1
if (-not $process) { "no window like $titleLike"; exit 1 }

[CaptureWindow]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 500

$rect = New-Object CaptureWindow+Rect
[CaptureWindow]::GetWindowRect($process.MainWindowHandle, [ref]$rect) | Out-Null

$bitmap = New-Object System.Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
$bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
