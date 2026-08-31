// agentd — agent 状态指示系统（产品版，Go 实现，单一原生二进制）
//
// 子命令：
//
//	agentd serve                 常驻桥：UDP 收事件 → 状态机 → 串口协议行 + 心跳 + STATUS? 应答
//	agentd forward <agent> <evt> 薄适配器：hook 直接调用，UDP 转发一条事件，毫秒级退出
//	agentd install               自安装：复制自身 + 写 agent 配置（pi/CC/Codex）+ 注册服务
//	agentd uninstall             反安装：删除服务与配置
//	agentd version               打印版本
//
// 串口协议（与 MCU 固件 app/tasks/task_status.c 完全兼容，勿改格式）：
//
//	PC→MCU: <agent>,<STATE>\n    STATE = IDLE/RUN/WAIT/DONE/FAIL
//	PC→MCU: HBT\n                心跳，每 2s；MCU 超过 6s 无下行判定断链
//	PC→MCU: SNAP,END\n           快照结束
//	MCU→PC: STATUS?\n            复位同步查询 → 重发全量快照 + SNAP,END
package main

import (
	"fmt"
	"os"
)

const version = "0.1.0"

const (
	udpPortNum  = 47831
	baudRate    = 115200
	appDirName  = "agentd"
	taskName    = "agent-status-bridge"
	serviceNote = "agent-status-bridge（agent 状态指示灯桥，Go 实现）"
)

func udpPort() int { return udpPortNum }

func usage() {
	fmt.Fprintf(os.Stderr, `agentd v%s — agent 状态指示灯系统

用法:
  agentd serve [--device COMx] [--dry-run] [--udp-port N]
  agentd forward <agent> <event>
  agentd install [--dry-run] [--no-service]
  agentd uninstall [--dry-run]
  agentd version

事件→状态映射（集中在 EVENT_MAP，agent 升级只改这里）:
  pi : agent_start→RUN  ui_prompt_start→WAIT  ui_prompt_end→RUN  agent_settled→DONE
  cc/cx : session_start→RUN  user_prompt_submit→RUN  stop→WAIT  stop_failure→FAIL  session_end→IDLE
`, version)
}

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(2)
	}
	var code int
	switch os.Args[1] {
	case "serve":
		code = cmdServe(os.Args[2:])
	case "forward":
		code = cmdForward(os.Args[2:])
	case "install":
		code = cmdInstall(os.Args[2:], false)
	case "uninstall":
		code = cmdInstall(os.Args[2:], true)
	case "version":
		fmt.Println(version)
		code = 0
	default:
		usage()
		code = 2
	}
	os.Exit(code)
}
