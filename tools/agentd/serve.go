package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"log"
	"net"
	"os"
	"os/signal"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"time"

	"go.bug.st/serial"
)

// ---------------------------------------------------------------- 桥状态

type bridge struct {
	states map[string]string // agent → STATE
	mu     sync.Mutex
	out    chan string // 串口写队列（单写者）
	port   serial.Port
	dryRun bool
}

func (b *bridge) snapshotLocked() []string {
	agents := knownAgents()
	lines := make([]string, 0, len(agents)+1)
	for _, a := range agents {
		if s, ok := b.states[a]; ok {
			lines = append(lines, a+","+s)
		}
	}
	lines = append(lines, "SNAP,END")
	return lines
}

func (b *bridge) send(line string) {
	select {
	case b.out <- line:
	default:
		log.Printf("[bridge] 写队列满，丢弃: %s", line)
	}
}

// writerLoop 唯一串口写者：心跳/事件/快照应答都经此串行写出
func (b *bridge) writerLoop() {
	for line := range b.out {
		payload := []byte(line + "\n")
		if b.dryRun {
			log.Printf("[tx] %s", line)
			continue
		}
		if b.port == nil {
			continue
		}
		if _, err := b.port.Write(payload); err != nil {
			log.Printf("[bridge] 串口写失败: %v", err)
		}
	}
}

func (b *bridge) onQuery(line string) {
	if line != "STATUS?" {
		return
	}
	b.mu.Lock()
	snap := b.snapshotLocked()
	b.mu.Unlock()
	for _, l := range snap {
		b.send(l)
	}
	log.Printf("[bridge] 响应 STATUS? 快照")
}

// readerLoop 读 MCU 上行（目前只有 STATUS? 复位同步查询）
// dry-run 模式下从 stdin 读行，方便无板调试
func (b *bridge) readerLoop() {
	if b.dryRun {
		sc := bufio.NewScanner(os.Stdin)
		for sc.Scan() {
			b.onQuery(strings.TrimSpace(sc.Text()))
		}
		return
	}
	if b.port == nil {
		return
	}
	buf := make([]byte, 64)
	acc := ""
	for {
		n, err := b.port.Read(buf)
		if err != nil {
			if strings.Contains(err.Error(), "timeout") {
				continue
			}
			log.Printf("[bridge] 串口读失败: %v", err)
			return
		}
		acc += string(buf[:n])
		for {
			idx := strings.IndexByte(acc, '\n')
			if idx < 0 {
				break
			}
			line := strings.TrimSpace(acc[:idx])
			acc = acc[idx+1:]
			b.onQuery(line)
		}
	}
}

// heartbeatLoop 每 2s 心跳；MCU 6s 无下行即判定断链
func (b *bridge) heartbeatLoop() {
	t := time.NewTicker(2 * time.Second)
	defer t.Stop()
	for range t.C {
		b.send("HBT")
	}
}

// udpLoop 收 forward 转发的事件
func (b *bridge) udpLoop(port int) error {
	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: port})
	if err != nil {
		return err
	}
	defer conn.Close()
	log.Printf("[bridge] UDP 监听 127.0.0.1:%d", port)
	buf := make([]byte, 1024)
	for {
		n, _, err := conn.ReadFromUDP(buf)
		if err != nil {
			return err
		}
		var msg struct {
			Agent string `json:"agent"`
			Event string `json:"event"`
		}
		if json.Unmarshal(buf[:n], &msg) != nil {
			continue
		}
		b.onEvent(msg.Agent, msg.Event)
	}
}

func (b *bridge) onEvent(agent, event string) {
	state := mapEvent(agent, event)
	if state == "" {
		log.Printf("[bridge] 未知事件: %s/%s", agent, event)
		return
	}
	b.mu.Lock()
	old := b.states[agent]
	changed := old != state
	if changed {
		b.states[agent] = state
	}
	b.mu.Unlock()
	if !changed {
		return
	}
	log.Printf("[event] %s %s -> %s", agent, event, state)
	b.send(agent + "," + state)
}

// ---------------------------------------------------------------- 串口探测

// detectPort 自动找 ST-Link 虚拟串口；找不到返回 ""。
// Windows 用 PowerShell 查 PnP 设备名（STLink/STMicro + (COMx)），
// Linux/macOS 依赖串口名 ttyACM/ttyUSB 兜底。
func detectPort() string {
	if runtimeIsWindows() {
		if p := detectPortWindows(); p != "" {
			return p
		}
	}
	ports, err := serial.GetPortsList()
	if err != nil {
		return ""
	}
	for _, p := range ports {
		low := strings.ToLower(p)
		if strings.Contains(low, "ttyacm") || strings.Contains(low, "ttyusb") {
			return p
		}
	}
	return ""
}

func runtimeIsWindows() bool {
	return runtime.GOOS == "windows"
}

// ---------------------------------------------------------------- serve

func cmdServe(args []string) int {
	fs := flag.NewFlagSet("serve", flag.ExitOnError)
	device := fs.String("device", "", "串口名（如 COM5）；缺省自动探测 ST-Link VCP")
	dryRun := fs.Bool("dry-run", false, "不碰串口，只打印协议行（调试用）")
	udpPort := fs.Int("udp-port", 47831, "UDP 监听端口")
	fs.Parse(args)

	b := &bridge{
		states: map[string]string{},
		out:    make(chan string, 32),
		dryRun: *dryRun,
	}

	port := *device
	if port == "" && !*dryRun {
		port = detectPort()
		if port == "" {
			log.Printf("[bridge] 未找到 ST-Link 虚拟串口，用 --device COMx 指定；无串口模式运行")
		}
	}
	if port != "" {
		p, err := serial.Open(port, &serial.Mode{BaudRate: baudRate})
		if err != nil {
			log.Printf("[bridge] 串口打开失败: %v（无串口模式运行）", err)
		} else {
			b.port = p
			log.Printf("[bridge] 串口打开: %s @ %d", port, baudRate)
		}
	} else if !*dryRun {
		log.Printf("[bridge] --dry-run 未指定且无串口，仅跟踪事件（不输出协议行）")
	}

	go b.writerLoop()
	go b.readerLoop()
	go b.heartbeatLoop()
	go func() {
		if err := b.udpLoop(*udpPort); err != nil {
			log.Printf("[bridge] UDP 退出: %v", err)
		}
	}()

	log.Printf("[bridge] ready（Ctrl+C 退出）")
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	<-sig
	close(b.out)
	if b.port != nil {
		_ = b.port.Close()
	}
	log.Printf("[bridge] bye")
	return 0
}
