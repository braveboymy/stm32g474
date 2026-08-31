package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

// ---------------------------------------------------------------- 配置合并

// hookCmd / hookGroup 对应 Claude Code 与 Codex 的 hooks 配置结构
type hookCmd struct {
	Type    string `json:"type"`
	Command string `json:"command"`
}

type hookGroup struct {
	Matcher string    `json:"matcher,omitempty"`
	Hooks   []hookCmd `json:"hooks"`
}

var hookEvents = []struct{ event, forwardEvent string }{
	{"SessionStart", "session_start"},
	{"UserPromptSubmit", "user_prompt_submit"},
	{"Stop", "stop"},
	{"StopFailure", "stop_failure"},
	{"SessionEnd", "session_end"},
}

// ourHookCommand 生成本系统的 hook 命令（调用自身二进制 forward 子命令）
func ourHookCommand(agent string) string {
	exe, err := os.Executable()
	if err != nil {
		exe = "agentd"
	}
	return `"` + filepath.ToSlash(exe) + `" forward ` + agent
}

// loadJSON 读取 JSON 到泛型 map（UseNumber 保留数值字面量，避免改写用户配置）
func loadJSON(path string) (map[string]any, error) {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return map[string]any{}, nil
		}
		return nil, err
	}
	defer f.Close()
	var m map[string]any
	dec := json.NewDecoder(f)
	dec.UseNumber()
	if err := dec.Decode(&m); err != nil {
		return nil, fmt.Errorf("%s 不是合法 JSON: %v", path, err)
	}
	return m, nil
}

func saveJSON(path string, m map[string]any) error {
	var buf bytes.Buffer
	enc := json.NewEncoder(&buf)
	enc.SetIndent("", "  ")
	enc.SetEscapeHTML(false)
	if err := enc.Encode(m); err != nil {
		return err
	}
	return os.WriteFile(path, buf.Bytes(), 0o644)
}

// mergeHooks 幂等地把本系统 hooks 合并进用户配置：命令去重、保留用户已有条目
func mergeHooks(m map[string]any, agent string) (map[string]any, error) {
	out := m
	rawHooks, ok := out["hooks"]
	if !ok {
		rawHooks = map[string]any{}
		out["hooks"] = rawHooks
	}
	hooks, ok := rawHooks.(map[string]any)
	if !ok {
		return nil, fmt.Errorf("hooks 字段类型异常（非对象）")
	}
	for _, h := range hookEvents {
		want := ourHookCommand(agent) + " " + h.forwardEvent
		rawList, ok := hooks[h.event]
		if !ok {
			rawList = []any{}
			hooks[h.event] = rawList
		}
		list, ok := rawList.([]any)
		if !ok {
			return nil, fmt.Errorf("hooks.%s 类型异常（非数组）", h.event)
		}
		if groupHasCommand(list, want) {
			continue
		}
		list = append(list, map[string]any{
			"matcher": "*",
			"hooks": []any{map[string]any{
				"type":    "command",
				"command": want,
			}},
		})
		hooks[h.event] = list
	}
	return out, nil
}

// removeOurHooks 删除本系统的 hook 条目（uninstall 用），其余保留
func removeOurHooks(m map[string]any, agent string) bool {
	rawHooks, ok := m["hooks"].(map[string]any)
	if !ok {
		return false
	}
	changed := false
	for _, h := range hookEvents {
		rawList, ok := rawHooks[h.event].([]any)
		if !ok {
			continue
		}
		want := ourHookCommand(agent) + " " + h.forwardEvent
		kept := make([]any, 0, len(rawList))
		for _, item := range rawList {
			if group, ok := item.(map[string]any); ok && groupHasCommand([]any{group}, want) {
				changed = true
				continue
			}
			kept = append(kept, item)
		}
		rawHooks[h.event] = kept
	}
	return changed
}

// groupHasCommand 判断 hookGroup 列表里是否已存在指定命令
func groupHasCommand(list []any, want string) bool {
	for _, item := range list {
		group, ok := item.(map[string]any)
		if !ok {
			continue
		}
		cmds, ok := group["hooks"].([]any)
		if !ok || len(cmds) == 0 {
			continue
		}
		if c, ok := cmds[0].(map[string]any); ok && c["command"] == want {
			return true
		}
	}
	return false
}

// ---------------------------------------------------------------- pi 扩展

const piExtTemplate = `// status-bridge: 把 pi 生命周期事件转发给常驻桥（UDP 127.0.0.1:%d）
// 由 agentd install 生成，改动会被覆盖；桥逻辑在 agentd 二进制内
const FWD = %s;

async function fire(event: string): Promise<void> {
  try {
    await pi.exec(FWD, ["forward", "pi", event]);
  } catch {
    /* 桥未启动时静默失败，不影响 pi 本身 */
  }
}

pi.on("agent_start", async () => { void fire("agent_start"); });
pi.on("ui_prompt_start", async () => { void fire("ui_prompt_start"); });
pi.on("ui_prompt_end", async () => { void fire("ui_prompt_end"); });
pi.on("agent_settled", async () => { void fire("agent_settled"); });
`

func piExtensionPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, ".pi", "agent", "extensions", "status-bridge.ts"), nil
}

// renderPiExtension 渲染扩展内容；exe 路径经 JSON 转义（引号/反斜杠安全，合法 TS 字符串）
func renderPiExtension() (string, error) {
	exe, err := os.Executable()
	if err != nil {
		exe = "agentd"
	}
	return fmt.Sprintf(piExtTemplate, udpPort(), jsonEscape(filepath.ToSlash(exe))), nil
}

func jsonEscape(s string) string {
	b, _ := json.Marshal(s)
	return string(b)
}

// ---------------------------------------------------------------- CLI

func cmdInstall(args []string, uninstall bool) int {
	fs := flag.NewFlagSet("install", flag.ExitOnError)
	dryRun := fs.Bool("dry-run", false, "只打印将执行的动作，不落盘")
	noService := fs.Bool("no-service", false, "跳过服务注册")
	_ = fs.Parse(args)

	home, err := os.UserHomeDir()
	if err != nil {
		fmt.Fprintf(os.Stderr, "无法获取用户目录: %v\n", err)
		return 1
	}

	// 1. pi 扩展
	piPath, _ := piExtensionPath()
	if *dryRun {
		fmt.Printf("[dry-run] pi 扩展: %s\n", piPath)
	} else {
		content, err := renderPiExtension()
		if err != nil {
			fmt.Fprintf(os.Stderr, "pi 扩展渲染失败: %v\n", err)
			return 1
		}
		dir := filepath.Dir(piPath)
		if err := os.MkdirAll(dir, 0o755); err != nil {
			fmt.Fprintf(os.Stderr, "pi 扩展目录创建失败: %v\n", err)
			return 1
		}
		if err := os.WriteFile(piPath, []byte(content), 0o644); err != nil {
			fmt.Fprintf(os.Stderr, "pi 扩展写入失败: %v\n", err)
			return 1
		}
		fmt.Printf("[ok] pi 扩展: %s（pi 启动自动加载，/reload 热重载）\n", piPath)
	}

	// 2/3. CC / Codex hooks 合并（幂等，保留用户已有条目）
	hooksTargets := []struct {
		path  string
		agent string
		label string
	}{
		{filepath.Join(home, ".claude", "settings.json"), "cc", "Claude Code"},
		{filepath.Join(home, ".codex", "hooks.json"), "cx", "Codex"},
	}
	for _, t := range hooksTargets {
		if *dryRun {
			fmt.Printf("[dry-run] %s hooks: %s\n", t.label, t.path)
			continue
		}
		m, err := loadJSON(t.path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "%s 配置读取失败: %v\n", t.label, err)
			return 1
		}
		var changed bool
		if uninstall {
			changed = removeOurHooks(m, t.agent)
		} else {
			if m, err = mergeHooks(m, t.agent); err != nil {
				fmt.Fprintf(os.Stderr, "%s hooks 合并失败: %v\n", t.label, err)
				return 1
			}
			changed = true
		}
		if changed {
			if err := os.MkdirAll(filepath.Dir(t.path), 0o755); err != nil {
				fmt.Fprintf(os.Stderr, "%s 目录创建失败: %v\n", t.label, err)
				return 1
			}
			if err := saveJSON(t.path, m); err != nil {
				fmt.Fprintf(os.Stderr, "%s hooks 写入失败: %v\n", t.label, err)
				return 1
			}
		}
		op := "合并"
		if uninstall {
			op = "清理"
		}
		fmt.Printf("[ok] %s hooks 已%s: %s\n", t.label, op, t.path)
	}

	// Codex hooks 特性开关（部分版本默认关闭）
	codexCfg := filepath.Join(home, ".codex", "config.toml")
	if !*dryRun && !uninstall {
		if data, err := os.ReadFile(codexCfg); err == nil {
			if !bytes.Contains(data, []byte("codex_hooks")) {
				f, err := os.OpenFile(codexCfg, os.O_APPEND|os.O_WRONLY|os.O_CREATE, 0o644)
				if err == nil {
					_, _ = io.WriteString(f, "\n[features]\ncodex_hooks = true\n")
					_ = f.Close()
					fmt.Printf("[ok] codex hooks 特性已开启: %s\n", codexCfg)
				}
			}
		}
	}

	// 4. 服务注册 / 卸载
	if *noService {
		fmt.Println("[skip] 服务注册已跳过（--no-service）")
	} else if *dryRun {
		fmt.Printf("[dry-run] 注册登录自启服务: %s\n", taskName)
	} else if uninstall {
		if err := unregisterService(); err != nil {
			fmt.Printf("[warn] 服务卸载失败: %v\n", err)
		} else {
			fmt.Printf("[ok] 服务已卸载: %s\n", taskName)
		}
	} else {
		if err := registerService(); err != nil {
			fmt.Printf("[warn] 服务注册失败: %v（可用 --no-service 跳过）\n", err)
		} else {
			fmt.Printf("[ok] 登录自启服务: %s\n", taskName)
		}
	}
	return 0
}
