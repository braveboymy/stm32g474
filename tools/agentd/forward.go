package main

import (
	"encoding/json"
	"fmt"
	"net"
	"os"
	"time"
)

// cmdForward 薄适配器：hook 每次事件调用一次，UDP 转发后立即退出。
// 桥未启动时静默失败（UDP 无连接），绝不阻塞 agent 本身。
func cmdForward(args []string) int {
	if len(args) != 2 {
		fmt.Fprintln(os.Stderr, "用法: agentd forward <agent> <event>")
		return 2
	}
	agent, event := args[0], args[1]

	payload, _ := json.Marshal(map[string]string{"agent": agent, "event": event})
	conn, err := net.DialTimeout("udp", fmt.Sprintf("127.0.0.1:%d", udpPortNum), 500*time.Millisecond)
	if err != nil {
		return 0 // 桥未运行：静默
	}
	defer conn.Close()
	// DialTimeout 后 UDP 无连接也会成功，这里给一次写超时保护
	_ = conn.SetWriteDeadline(time.Now().Add(200 * time.Millisecond))
	_, _ = conn.Write(payload)
	return 0
}
