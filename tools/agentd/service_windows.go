//go:build windows

package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

// registerService：复制自身到稳定目录 + 注册登录自启任务（schtasks onlogon）
func registerService() error {
	exe, err := os.Executable()
	if err != nil {
		return err
	}
	target := filepath.Join(os.Getenv("LOCALAPPDATA"), "agentd", "agentd.exe")
	if err := selfCopy(exe, target); err != nil {
		return err
	}
	cmd := fmt.Sprintf(`"%s" serve`, target)
	if err := exec.Command("schtasks", "/create", "/tn", taskName,
		"/tr", cmd, "/sc", "onlogon", "/f").Run(); err != nil {
		return fmt.Errorf("schtasks: %v", err)
	}
	return nil
}

func unregisterService() error {
	if err := exec.Command("schtasks", "/delete", "/tn", taskName, "/f").Run(); err != nil {
		return fmt.Errorf("schtasks: %v", err)
	}
	fmt.Printf("[ok] 提示：如不再需要，可手动删除 %s\n",
		filepath.Join(os.Getenv("LOCALAPPDATA"), "agentd"))
	return nil
}

// selfCopy 复制自身（服务任务引用 %LOCALAPPDATA%\agentd\agentd.exe 稳定路径）
func selfCopy(src, dst string) error {
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	data, err := os.ReadFile(src)
	if err != nil {
		return err
	}
	if abs(src) == abs(dst) {
		return nil
	}
	return os.WriteFile(dst, data, 0o755)
}

func abs(p string) string {
	a, err := filepath.Abs(p)
	if err != nil {
		return p
	}
	return a
}
