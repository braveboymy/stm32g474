//go:build !windows

package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
)

// Linux：用户级 systemd unit；macOS：LaunchAgent plist。
// 两者都不需要 root，登录后自动运行。

func registerService() error {
	exe, err := os.Executable()
	if err != nil {
		return err
	}
	switch runtime.GOOS {
	case "linux":
		unit := `[Unit]
Description=` + serviceNote + `
After=network.target

[Service]
ExecStart=` + exe + ` serve
Restart=on-failure

[Install]
WantedBy=default.target
`
		dir := filepath.Join(homeDir(), ".config", "systemd", "user")
		path := filepath.Join(dir, taskName+".service")
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return err
		}
		if err := os.WriteFile(path, []byte(unit), 0o644); err != nil {
			return err
		}
		_ = exec.Command("systemctl", "--user", "daemon-reload").Run()
		return exec.Command("systemctl", "--user", "enable", "--now", taskName+".service").Run()
	case "darwin":
		plist := `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>com.agentd.status</string>
  <key>ProgramArguments</key>
  <array>
    <string>` + exe + `</string>
    <string>serve</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
</dict>
</plist>
`
		dir := filepath.Join(homeDir(), "Library", "LaunchAgents")
		path := filepath.Join(dir, "com.agentd.status.plist")
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return err
		}
		if err := os.WriteFile(path, []byte(plist), 0o644); err != nil {
			return err
		}
		return exec.Command("launchctl", "load", path).Run()
	default:
		return fmt.Errorf("暂不支持平台: %s（可用 --no-service 跳过）", runtime.GOOS)
	}
}

func unregisterService() error {
	switch runtime.GOOS {
	case "linux":
		_ = exec.Command("systemctl", "--user", "disable", "--now", taskName+".service").Run()
		return os.Remove(filepath.Join(homeDir(), ".config", "systemd", "user", taskName+".service"))
	case "darwin":
		path := filepath.Join(homeDir(), "Library", "LaunchAgents", "com.agentd.status.plist")
		_ = exec.Command("launchctl", "unload", path).Run()
		return os.Remove(path)
	default:
		return nil
	}
}

func homeDir() string {
	h, err := os.UserHomeDir()
	if err != nil {
		return "."
	}
	return h
}
