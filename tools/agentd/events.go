package main

// 事件→状态映射：forward 只透传事件名，映射集中在本文件，agent 升级只改这里。
// MCU 侧接收的是 STATE 枚举（IDLE/RUN/WAIT/DONE/FAIL），见 task_status.c。

var commonEventMap = map[string]string{
	"session_start":      "RUN",
	"user_prompt_submit": "RUN",
	"stop":               "WAIT",
	"stop_failure":       "FAIL",
	"session_end":        "IDLE",
}

var eventMap = map[string]map[string]string{
	"pi": {
		"agent_start":     "RUN",
		"ui_prompt_start": "WAIT",
		"ui_prompt_end":   "RUN",
		"agent_settled":   "DONE",
	},
	"cc": commonEventMap,
	"cx": commonEventMap,
}

// mapEvent 把 (agent, event) 归一化为状态；未知返回空串。
func mapEvent(agent, event string) string {
	m, ok := eventMap[agent]
	if !ok {
		return ""
	}
	return m[event]
}

// knownAgents 返回事件表里注册过的 agent 名（用于 STATUS? 快照应答顺序）。
func knownAgents() []string {
	// 固定顺序输出，保证 MCU 日志可读性
	return []string{"pi", "cc", "cx"}
}
