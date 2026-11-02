param([string]$Port = '', [switch]$SelfTest)
$ErrorActionPreference = 'Stop'

# ปรับหน้าตาได้ที่นี่ ไม่เปลี่ยนกติกาใน firmware
$DashboardTitle = 'SMART HOME | DEVICE DASHBOARD'
$LabelWidth = 22
$RefreshMs = 200
$StaleSeconds = 4

# รับข้อมูลให้ครบหนึ่งชุดก่อนเปลี่ยนทุกแถวบนหน้าจอ
function New-DashboardState {
    return @{ Buffer=''; Pending=$null; Snapshot=$null; Firmware='Not reported by board';
              DisplayTest=''; Event='Waiting for telemetry'; Frames=0 }
}
function Receive-DashboardText($State, [string]$Text) {
    $State.Buffer += $Text.Replace("`r", '')
    while (($end = $State.Buffer.IndexOf([char]10)) -ge 0) {
        $line = $State.Buffer.Substring(0,$end)
        $State.Buffer = $State.Buffer.Substring($end+1)
        # ไม่ให้ข้อความจาก serial ส่ง escape sequence ไปควบคุม terminal
        $line = [regex]::Replace($line, '[\x00-\x1f\x7f]', '')
        if ($line -eq '--- LIVE STATUS ---') { $State.Pending = @{}; continue }
        if ($line -match '^FW=(.+)$') { $State.Firmware=$Matches[1]; continue }
        if ($line -match '^POLICY=(.+)$') { if ($null -ne $State.Pending) { $State.Pending.Policy=$Matches[1] }; continue }
        if ($line -match '^TEMP SENSOR=(.+)$') { if ($null -ne $State.Pending) { $State.Pending.TempModel=$Matches[1] }; continue }
        if ($line -match '^SEG CMD=(\d+) GPIO=(\d+) MAP=(.+)$') { if ($null -ne $State.Pending) { $State.Pending.Segment=@($Matches[1],$Matches[2],$Matches[3]) }; continue }
        if ($line -match '^DISPLAY CODE=(\d+)') { $State.DisplayTest="TEST DIGIT $($Matches[1]) (energy paused)" }
        if ($line -match '^DISPLAY TEST:') { $State.DisplayTest='TEST STARTING (energy paused)' }
        if ($line -match '^DISPLAY TEST END') { $State.DisplayTest='' }
        if ($line -match '^BAT=(\d+)/9 energy=(\d+)/(\d+) SOC=([\d.]+)% temp=(NA|-?[\d.]+)C target=(\d+)C AC\[(AUTO|MANUAL),(ON|OFF)\] LIGHT\[(AUTO|MANUAL),(ON|OFF)\] FAN=(ON|OFF) ECO=(ON|OFF) charge/cost=(\d+)/(\d+) ADC\(T,L,P\)=(\d+),(\d+),(\d+)$') {
            if ($null -ne $State.Pending) {
                $State.Pending.Core = @($Matches[1],$Matches[2],$Matches[3],$Matches[4],$Matches[5],$Matches[6],$Matches[7],$Matches[8],$Matches[9],$Matches[10],$Matches[11],$Matches[12],$Matches[13],$Matches[14],$Matches[15],$Matches[16],$Matches[17])
            }
            continue
        }
        if ($line -match '^BATTERY: (.+) \| Solar input=(\d+) units/s \| Load=(\d+) units/s$') {
            if ($null -ne $State.Pending) { $State.Pending.Power=@($Matches[1],$Matches[2],$Matches[3]) }
            continue
        }
        if ($line -match '^Brightness=(\d+) \(([^)]+)\); DMA samples=(\d+) \| LDR (normal|inverted)$') {
            if ($null -ne $State.Pending -and $State.Pending.ContainsKey('Core') -and $State.Pending.ContainsKey('Power')) {
                $State.Pending.Light=@($Matches[1],$Matches[2],$Matches[3],$Matches[4])
                $State.Pending.At=Get-Date
                $State.DisplayTest=''
                $State.Snapshot=$State.Pending
                $State.Frames++
            }
            $State.Pending=$null
            continue
        }
        if ($line -match '^\[|^DISPLAY |^ADC ERROR:|^POWER SAVING:') { $State.Event=$line }
    }
    if ($State.Buffer.Length -gt 8192) { $State.Buffer=''; $State.Pending=$null; $State.Event='Discarded incomplete serial data; waiting for next frame' }
}
function Get-DashboardLines($State, [string]$PortName) {
    $s=$State.Snapshot
    $link='WAITING FOR COMPLETE STATUS'
    if ($null -ne $s) {
        $age=[int]((Get-Date)-$s.At).TotalSeconds
        $link=if ($age -ge $StaleSeconds) { "STALE - no complete update for $age s" } else { 'LIVE' }
    }
    $rows = @(
        "$DashboardTitle"
        "Connection : $link"
        ('-' * 77)
        ('DEVICE / SENSOR'.PadRight($LabelWidth) + 'STATUS / VALUE')
        ('-' * 77)
    )
    if ($null -eq $s) {
        foreach ($name in 'BATTERY','AIR CONDITIONER','LIGHT','FAN','ECO MODE','TEMPERATURE','SOLAR INPUT','LOAD OUTPUT') { $rows += $name.PadRight($LabelWidth)+'-- waiting --' }
    } else {
        $c=$s.Core; $p=$s.Power
        $rows += 'BATTERY'.PadRight($LabelWidth)+"$($c[0])/9 | $($c[3])% | $($p[0])"
        $rows += 'AIR CONDITIONER'.PadRight($LabelWidth)+"$($c[7]) | $($c[6]) | Target $($c[5]) C"
        $rows += 'LIGHT'.PadRight($LabelWidth)+"$($c[9]) | $($c[8])"
        $rows += 'FAN'.PadRight($LabelWidth)+$c[10]
        # ใช้ POLICY ที่บอร์ดส่งมา ไม่ใช้ชื่อหน้าต่างยืนยันว่า firmware เป็น1.4
        $ecoText = if ([int]$c[0] -eq 0) { 'EMPTY | ALL LOADS LOCKED' }
                   elseif ($c[11] -eq 'OFF') { 'NORMAL' }
                   elseif ($s.Policy -eq 'ECO_ENTRY_OFF_REOPEN_ALLOWED') { 'ECO | REOPEN ALLOWED (A/F/M)' }
                   else { 'ECO | board policy not reported' }
        $rows += 'ECO MODE'.PadRight($LabelWidth)+$ecoText
        $tempLabel=if ($c[4] -eq 'NA') { 'UNAVAILABLE' } elseif ($s.TempModel -match 'ESTIMATE') { "$($c[4]) C ESTIMATED" } else { "$($c[4]) C" }
        $rows += 'TEMPERATURE'.PadRight($LabelWidth)+"$tempLabel | ADC $($c[14])"
        $rows += 'SOLAR INPUT'.PadRight($LabelWidth)+"$($p[1]) units/s"
        $rows += 'LOAD OUTPUT'.PadRight($LabelWidth)+"$($p[2]) units/s"
    }
    $rows += ('-' * 77)
    if ($null -ne $s) { $rows += "Updated: $($s.At.ToString('HH:mm:ss')) | Frame $($State.Frames) | Energy $($s.Core[1])/$($s.Core[2]) | DMA $($s.Light[2])" }
    else { $rows += 'Updated: --' }
    $rows += "Latest: $($State.Event)"
    $rows += 'A=AC L=Light F=Fan M=Mode X=All off I=Invert LDR ?=Status'
    $rows += 'Z=Empty battery Q=Quit Program'
    return $rows
}
function Test-Dashboard {
    $s=New-DashboardState
    $frame="--- LIVE STATUS ---`r`nFW=TEST_FW`r`nBAT=9/9 energy=540000/540000 SOC=100.0% temp=25.3C target=24C AC[MANUAL,OFF] LIGHT[AUTO,ON] FAN=OFF ECO=OFF charge/cost=200/100 ADC(T,L,P)=310,2500,2048`r`nBATTERY: FULL | Solar input=2000 units/s | Load=1000 units/s`r`nBrightness=2500 (solar starts above 900); DMA samples=45 | LDR normal`r`n"
    for ($i=0; $i -lt $frame.Length-1; $i++) { Receive-DashboardText $s $frame.Substring($i,1) }
    if ($null -ne $s.Snapshot) { throw 'Rendered before final newline' }
    Receive-DashboardText $s "`n"
    if ($s.Frames -ne 1 -or $s.Snapshot.Core[0] -ne '9') { throw 'Chunked frame parsing failed' }
    Receive-DashboardText $s "--- LIVE STATUS ---`nBAT=2/9 truncated`nBrightness=1 (solar starts below 900); DMA samples=46 | LDR normal`n"
    if ($s.Frames -ne 1 -or $s.Snapshot.Core[0] -ne '9') { throw 'Incomplete frame replaced good snapshot' }
    Receive-DashboardText $s ($frame.Replace('BAT=9/9','BAT=8/9').Replace('FW=TEST_FW'+"`r`n",''))
    if ($s.Frames -ne 2 -or $s.Snapshot.Core[0] -ne '8') { throw 'Legacy firmware frame failed' }
    Receive-DashboardText $s ($frame.Replace('temp=25.3C','temp=NAC'))
    if ($s.Snapshot.Core[4] -ne 'NA') { throw 'Invalid temperature frame lost' }
    Receive-DashboardText $s ($frame.Replace('temp=25.3C','temp=-5.2C'))
    if ($s.Snapshot.Core[4] -ne '-5.2') { throw 'Negative temperature frame lost' }
    $newFrame=$frame.Replace('FW=TEST_FW',"FW=TEST_FW`nTEMP SENSOR=NTC ESTIMATE`nSEG CMD=9 GPIO=9 MAP=PC7,PA8,PB10,PB9")
    Receive-DashboardText $s $newFrame
    if (((Get-DashboardLines $s COM_TEST) -join "`n") -notmatch '25.3 C ESTIMATED') { throw 'Temperature estimate label missing' }
    if ($s.Snapshot.Segment[1] -ne '9') { throw 'GPIO parsing failed' }
    $rows=Get-DashboardLines $s COM_TEST
    if (($rows -join "`n") -notmatch 'AIR CONDITIONER\s+OFF \| MANUAL') { throw 'AC row mapping failed' }
    if (($rows -join "`n") -notmatch 'SOLAR INPUT\s+2000 units/s') { throw 'Solar row mapping failed' }
    $s.Snapshot.At=(Get-Date).AddSeconds(-6)
    if (((Get-DashboardLines $s COM_TEST) -join "`n") -notmatch 'STALE') { throw 'Stale data not indicated' }
    $ecoFrame=$frame.Replace('BAT=9/9','BAT=3/9').Replace('ECO=OFF','ECO=ON').Replace('FW=TEST_FW',"FW=TEST_FW`nPOLICY=ECO_ENTRY_OFF_REOPEN_ALLOWED")
    Receive-DashboardText $s $ecoFrame
    if (((Get-DashboardLines $s COM_TEST) -join "`n") -notmatch 'REOPEN ALLOWED') { throw 'V1.4 Eco policy not rendered' }
    Receive-DashboardText $s ($ecoFrame.Replace('BAT=3/9','BAT=0/9'))
    if (((Get-DashboardLines $s COM_TEST) -join "`n") -notmatch 'ALL LOADS LOCKED') { throw 'Empty must override reopen label' }
    Receive-DashboardText $s ($frame.Replace('BAT=9/9','BAT=3/9').Replace('ECO=OFF','ECO=ON'))
    if (((Get-DashboardLines $s COM_TEST) -join "`n") -match 'REOPEN ALLOWED') { throw 'Old firmware must not inherit v1.4 policy' }
    Write-Output 'PASS: fragmented/complete/invalid/legacy frames, row mapping, temperature validity and estimate labels, GPIO diagnostics, stale status'
}
if ($SelfTest) { Test-Dashboard; return }
if ([Console]::IsOutputRedirected -or [Console]::IsInputRedirected) { throw 'Run in an interactive PowerShell terminal (not output redirection).' }
if (!$Port) {
    $ports=@(Get-CimInstance Win32_SerialPort | Where-Object { $_.Name -match 'STLink|ST-LINK|STM32' })
    if ($ports.Count -ne 1) { throw 'Specify board port: ./live_status.ps1 -Port COM3' }
    $Port=$ports[0].DeviceID
}
$serial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$state=New-DashboardState
$oldCursor=[Console]::CursorVisible
$lastWidth=0; $lastHeight=0; $lastPaint=[DateTime]::MinValue
try {
    $serial.Open()
    [Console]::CursorVisible=$false
    $serial.Write('v?')
    while ($true) {
        if ([Console]::KeyAvailable) {
            $key=[Console]::ReadKey($true).KeyChar
            if ($key -eq 'q' -or $key -eq 'Q') { break }
            if ('aAlLfFmMxXiIhHzZdDvV?01234'.Contains([string]$key)) { $serial.Write([string]$key); $state.Event="Command sent: $key" }
        }
        $incoming=$serial.ReadExisting()
        if ($incoming.Length -gt 0) { Receive-DashboardText $state $incoming }
        if (((Get-Date)-$lastPaint).TotalMilliseconds -ge $RefreshMs) {
            $width=[Console]::WindowWidth; $height=[Console]::WindowHeight
            if ($width -ne $lastWidth -or $height -ne $lastHeight) { [Console]::Clear(); $lastWidth=$width; $lastHeight=$height }
            $lines=@(Get-DashboardLines $state $Port)
            if ($width -lt 78 -or $height -lt 25) { $lines=@('Please enlarge terminal to at least 78 columns x 25 rows.', "Current: $width x $height | Q=Quit (serial reception continues)") }
            $paint=@()
            for ($r=0; $r -lt $height-1; $r++) {
                $line=if ($r -lt $lines.Count) { [string]$lines[$r] } else { '' }
                if ($line.Length -gt $width-1) { $line=$line.Substring(0,[Math]::Max(0,$width-4))+'...' }
                $paint+=$line.PadRight([Math]::Max(1,$width-1))
            }
            [Console]::SetCursorPosition(0,0)
            # เขียนทั้งเฟรมครั้งเดียว ไม่พิมพ์ UART ทีละบรรทัดและไม่เลื่อนจอ
            [Console]::Write(($paint -join "`r`n"))
            $lastPaint=Get-Date
        }
        Start-Sleep -Milliseconds 25
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
    [Console]::CursorVisible=$oldCursor
    [Console]::Clear()
    Write-Host "Serial port $Port released."
}


