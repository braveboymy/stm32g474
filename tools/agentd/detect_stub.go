//go:build !windows

package main

// detectPortWindows 仅 Windows 实现（见 detect_windows.go）；其他平台空实现。
func detectPortWindows() string {
	return ""
}
