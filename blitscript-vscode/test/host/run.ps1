# starts a real VS Code with a throwaway profile and runs the suites inside it
# run.ps1 <engine repo> <blit-tools repo> <folder with a blitscriptc.exe for the first suite> [VS Code to test in]
param($engine, $tools, $compilerFolder, $codeExecutable)

$extension = (Resolve-Path "$PSScriptRoot\..\..").Path
# a new folder every run: VS Code restores a workspace's last session by its path, over the running tests
$work = Join-Path $env:TEMP "blitscript-host-test-$(Get-Date -Format 'yyyyMMddHHmmss')"
"work folder: $work"

$workspace = Join-Path $work 'workspace'
New-Item -ItemType Directory -Force "$workspace\blitscript", "$workspace\docs", "$workspace\.vscode", "$work\projects" | Out-Null
Copy-Item "$engine\blitscript\engine.bs" "$workspace\blitscript\"
Copy-Item "$engine\docs\cheatsheet.txt" "$workspace\docs\"

$test = @'
INCLUDE "engine.bs"
MAIN LBL main
SET a 3
LBL top
CALL DrawCube VEC3 0 1 0, VEC3 1 1 1, RED
CALL ClearBackground COLOR 24 26 32 255
SUB a 1
JNE top
CALL helper a

END

FN helper value
    CALL log value
END
'@
[IO.File]::WriteAllText("$workspace\blitscript\test.bs", $test)
[IO.File]::WriteAllText("$workspace\blitscript\broken.bs", "MAIN LBL main`nSET v VEC3 1 2 3`nCMP v, VEC3 1 2 3`nEND`n")

$settings = @{ 'blitscript.compilerPath' = "$compilerFolder\blitscriptc.exe" } | ConvertTo-Json
[IO.File]::WriteAllText("$workspace\.vscode\settings.json", $settings)

$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
$env:ENGINE_FOLDER = $engine
$env:TOOLS_FOLDER = $tools
$env:PROJECTS_FOLDER = "$work\projects"
$env:HOMEINVASION_SCREENSHOT = "$work\game.bmp"

$code = if ($codeExecutable) { $codeExecutable } else { Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\Code.exe' }

function RunSuite($folder, $suite, $results, $seconds) {
    $env:RESULTS_FILE = $results
    $arguments = @(
        "`"$folder`"",
        "--extensionDevelopmentPath=`"$extension`"",
        "--extensionTestsPath=`"$extension\test\host\$suite`"",
        "--user-data-dir=`"$work\user`"",
        "--extensions-dir=`"$work\extensions`"",
        '--disable-workspace-trust',
        '--skip-welcome',
        '--new-window'
    )

    $process = Start-Process -FilePath $code -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit($seconds * 1000)) { Stop-Process -Id $process.Id -Force -Confirm:$false; "$suite TIMED OUT" }

    if (Test-Path $results) {
        $lines = Get-Content $results
        $lines
        "$suite`: $(@($lines | Where-Object { $_ -like 'PASS*' }).Count) passed, $(@($lines | Where-Object { $_ -like 'FAIL*' }).Count) failed"
    } else {
        "$suite`: NO RESULTS, the suite never finished"
    }
}

RunSuite $workspace 'suite.js' "$work\results.txt" 120
# set SKIP_PROJECT_SUITE to leave out the engine build, which takes minutes
if (-not $env:SKIP_PROJECT_SUITE) {
    RunSuite "$work\projects\Host Game" 'projectSuite.js' "$work\project-results.txt" 420
}
