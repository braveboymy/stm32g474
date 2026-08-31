//go:build windows

package main

import (
	"os/exec"
	"regexp"
	"strings"
)

// detectPortWindows：用 PowerShell 查 PnP 设备，匹配 STLink/STMicro 的 (COMx) 串口。
// ST-Link VCP 的设备名形如 "STMicroelectronics STLink Virtual COM Port (COM5)"。
func detectPortWindows() string {
	ps := `Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'STLink|STMicro' } | ForEach-Object { if ($_.Name -match '\(COM\d+\)') { $matches[0] } }`
	cmd := exec.Command("powershell", "-NoProfile", "-Command", ps)
	out, err := cmd.Output()
	if err != nil {
		return ""
	}
	re := regexp.MustCompile(`\(COM\d+\)`)
	for _, line := range strings.Split(string(out), "\n") {
		if m := re.FindString(line); m != "" {
			return strings.Trim(m, "()")
		}
	}
	return ""
}
