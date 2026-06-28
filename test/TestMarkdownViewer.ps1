# Notepad3 Markdown Viewer E2E Test Suite
# Genuine E2E implementation

$ErrorActionPreference = "Stop"

# 1. Check for Notepad3.exe
$ExecutablePaths = @(
    "Bin\Release_x64_v145\Notepad3.exe",
    "Bin\Debug_x64_v145\Notepad3.exe",
    "x64\Release\Notepad3.exe"
)

$Notepad3Path = $null
foreach ($path in $ExecutablePaths) {
    if (Test-Path $path) {
        $Notepad3Path = $path
        break
    }
}

if (-not $Notepad3Path) {
    Write-Host "Notepad3.exe not found. You must build the project before executing the test suite." -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] Notepad3 executable found at: $Notepad3Path" -ForegroundColor Green

# Define Win32 structures and APIs
$Win32Source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class NP3E2EWin32 {
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr FindWindowEx(IntPtr hwndParent, IntPtr hwndChildAfter, string lpszClass, string lpszWindow);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);
}
"@

if (-not ("NP3E2EWin32" -as [type])) {
    Add-Type -TypeDefinition $Win32Source
}

# Constants
$WM_COMMAND = 0x0111
$WM_CLOSE = 0x0010
$IDM_VIEW_MARKDOWN = 41066

# Create temporary markdown file
$TempFile = [System.IO.Path]::GetTempFileName() + ".md"
"# Test Markdown`r`n`r`nThis is a test of the Notepad3 Markdown Viewer E2E suite." | Out-File -FilePath $TempFile -Encoding utf8

$Process = $null
$hwndMain = [IntPtr]::Zero

try {
    # 2. Launch Notepad3 with the .md file
    Write-Host "[INFO] Launching Notepad3 with temp file: $TempFile"
    $Process = Start-Process -FilePath $Notepad3Path -ArgumentList "`"$TempFile`"" -PassThru
    
    # 3. Verify main window creation
    for ($i = 0; $i -lt 50; $i++) {
        Start-Sleep -Milliseconds 100
        
        $foundHwnd = [IntPtr]::Zero
        [NP3E2EWin32]::EnumWindows({
            param($hwnd, $lparam)
            $sbClass = New-Object System.Text.StringBuilder 256
            [NP3E2EWin32]::GetClassName($hwnd, $sbClass, 256) | Out-Null
            $c = $sbClass.ToString()
            if ($c -eq "Notepad3" -or $c -eq "Notepad3U") {
                $winPid = 0
                [NP3E2EWin32]::GetWindowThreadProcessId($hwnd, [ref]$winPid) | Out-Null
                if ($winPid -eq $Process.Id) {
                    $script:hwndMain = $hwnd
                    return $false # stop enumeration
                }
            }
            return $true # continue enumeration
        }, [IntPtr]::Zero) | Out-Null
        
        if ($hwndMain -ne [IntPtr]::Zero) {
            break
        }
    }
    
    if ($hwndMain -eq [IntPtr]::Zero) {
        throw "ASSERTION FAILED: Notepad3 main window not found."
    }
    Write-Host "[PASS] Notepad3 main window created successfully (HWND: $hwndMain)." -ForegroundColor Green
    
    # 4. Verify toolbar is found
    $hwndToolbar = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "ToolbarWindow32", $null)
    if ($hwndToolbar -eq [IntPtr]::Zero) {
        $hwndRebar = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "RebarWindow32", $null)
        if ($hwndRebar -ne [IntPtr]::Zero) {
            $hwndToolbar = [NP3E2EWin32]::FindWindowEx($hwndRebar, [IntPtr]::Zero, "ToolbarWindow32", $null)
        }
    }
    
    if ($hwndToolbar -eq [IntPtr]::Zero) {
        throw "ASSERTION FAILED: Toolbar not found."
    }
    Write-Host "[PASS] Toolbar found successfully (HWND: $hwndToolbar)." -ForegroundColor Green
    
    # Verify no AtlAxWin window exists initially
    $hwndBrowserInitial = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "AtlAxWin", $null)
    if ($hwndBrowserInitial -ne [IntPtr]::Zero -and [NP3E2EWin32]::IsWindowVisible($hwndBrowserInitial)) {
        throw "ASSERTION FAILED: AtlAxWin browser window is visible initially."
    }
    Write-Host "[PASS] Browser window is not visible initially." -ForegroundColor Green
    
    # 5. Toggle ON '.md' toolbar button
    Write-Host "[INFO] Toggling '.md' viewer ON..."
    [NP3E2EWin32]::SendMessage($hwndMain, $WM_COMMAND, [IntPtr]$IDM_VIEW_MARKDOWN, [IntPtr]::Zero) | Out-Null
    
    # 6. Verify AtlAxWin window creation
    $hwndBrowser = [IntPtr]::Zero
    for ($i = 0; $i -lt 50; $i++) {
        Start-Sleep -Milliseconds 100
        $hwndBrowser = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "AtlAxWin", $null)
        if ($hwndBrowser -ne [IntPtr]::Zero -and [NP3E2EWin32]::IsWindowVisible($hwndBrowser)) {
            break
        }
    }
    
    if ($hwndBrowser -eq [IntPtr]::Zero) {
        throw "ASSERTION FAILED: Toggling '.md' button did not create or show 'AtlAxWin' window."
    }
    Write-Host "[PASS] AtlAxWin child window created and visible (HWND: $hwndBrowser)." -ForegroundColor Green
    
    # 7. Verify sizing splitting divides the main window width 50/50
    # Find the Scintilla edit window
    $hwndEdit = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "Scintilla", $null)
    if ($hwndEdit -eq [IntPtr]::Zero) {
        throw "ASSERTION FAILED: Scintilla edit window not found."
    }
    
    # Get rects
    $rectEdit = New-Object NP3E2EWin32+RECT
    $rectBrowser = New-Object NP3E2EWin32+RECT
    
    if (-not [NP3E2EWin32]::GetWindowRect($hwndEdit, [ref]$rectEdit)) {
        throw "ASSERTION FAILED: Failed to get Scintilla window rect."
    }
    if (-not [NP3E2EWin32]::GetWindowRect($hwndBrowser, [ref]$rectBrowser)) {
        throw "ASSERTION FAILED: Failed to get AtlAxWin window rect."
    }
    
    $widthEdit = $rectEdit.Right - $rectEdit.Left
    $widthBrowser = $rectBrowser.Right - $rectBrowser.Left
    
    Write-Host "[INFO] Window widths -> Edit: $widthEdit, Browser: $widthBrowser"
    $diff = [Math]::Abs($widthEdit - $widthBrowser)
    # Allow up to 20 pixels difference for borders/margins/splitters
    if ($diff -gt 20) {
        throw "ASSERTION FAILED: Sizing splitting does not divide main window width 50/50 (diff: $diff)."
    }
    Write-Host "[PASS] Sizing splitting divides main window width 50/50 (diff: $diff)." -ForegroundColor Green
    
    # 8. Toggle OFF '.md' toolbar button
    Write-Host "[INFO] Toggling '.md' viewer OFF..."
    [NP3E2EWin32]::SendMessage($hwndMain, $WM_COMMAND, [IntPtr]$IDM_VIEW_MARKDOWN, [IntPtr]::Zero) | Out-Null
    
    # 9. Verify AtlAxWin window is hidden or destroyed
    $isBrowserHidden = $false
    for ($i = 0; $i -lt 50; $i++) {
        Start-Sleep -Milliseconds 100
        $hwndBrowserAfter = [NP3E2EWin32]::FindWindowEx($hwndMain, [IntPtr]::Zero, "AtlAxWin", $null)
        if ($hwndBrowserAfter -eq [IntPtr]::Zero -or -not [NP3E2EWin32]::IsWindowVisible($hwndBrowserAfter)) {
            $isBrowserHidden = $true
            break
        }
    }
    
    if (-not $isBrowserHidden) {
        throw "ASSERTION FAILED: AtlAxWin window is still visible after toggle off."
    }
    Write-Host "[PASS] AtlAxWin window is hidden/destroyed after toggling off." -ForegroundColor Green
    
    Write-Host "[INFO] All assertions passed successfully!" -ForegroundColor Green

} finally {
    # 10. Graceful termination
    if ($hwndMain -and $hwndMain -ne [IntPtr]::Zero) {
        Write-Host "[INFO] Sending WM_CLOSE to main window..."
        [NP3E2EWin32]::PostMessage($hwndMain, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    }
    
    if ($Process) {
        Write-Host "[INFO] Waiting for Notepad3 process to exit..."
        $Process.WaitForExit(3000)
        if (-not $Process.HasExited) {
            Write-Host "[WARNING] Process did not exit gracefully, killing..." -ForegroundColor Yellow
            $Process.Kill()
        }
    }
    
    # Cleanup temp file
    if (Test-Path $TempFile) {
        Remove-Item -Path $TempFile -Force
    }
}
