package main

import (
	"bufio"
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"
)

var resultPattern = regexp.MustCompile(`^TEST (PASS|FAIL) \(([^)]+)\) in ([0-9]+) ms$`)

type config struct {
	buildDir      string
	picotool      string
	port          string
	pattern       string
	timeout       time.Duration
	resultGrace   time.Duration
	loadTimeout   time.Duration
	baud          int
	pollInterval  time.Duration
	postRebootGap time.Duration
}

type testResult struct {
	status    string
	name      string
	elapsedMs string
	line      string
}

func main() {
	cfg := parseFlags()

	uf2s, err := discoverUF2s(cfg.buildDir, cfg.pattern)
	if err != nil {
		fail(err)
	}
	if len(uf2s) == 0 {
		fail(fmt.Errorf("no UF2 files found in %s matching %q", cfg.buildDir, cfg.pattern))
	}

	for _, uf2 := range uf2s {
		fmt.Printf("==> Running %s\n", filepath.Base(uf2))

		if err := loadUF2(cfg, uf2); err != nil {
			fail(fmt.Errorf("load failed for %s: %w", filepath.Base(uf2), err))
		}

		portPath, err := waitForSerialAfterLoad(cfg)
		if err != nil {
			fail(fmt.Errorf("serial port not ready for %s: %w", filepath.Base(uf2), err))
		}

		result, err := waitForResult(cfg, portPath)
		if err != nil {
			fail(fmt.Errorf("did not receive result for %s: %w", filepath.Base(uf2), err))
		}

		fmt.Printf("    %s\n", result.line)
		if result.status != "PASS" {
			fail(fmt.Errorf("test failed: %s", result.name))
		}
	}

	fmt.Println("All Pico tests passed")
}

func waitForSerialAfterLoad(cfg config) (string, error) {
	time.Sleep(cfg.postRebootGap)

	portPath, err := waitForSerialPort(cfg)
	if err == nil {
		return portPath, nil
	}

	fmt.Fprintf(os.Stderr, "WARN: serial port not ready after load, retrying with reboot: %v\n", err)
	if rebootErr := rebootApplication(cfg); rebootErr != nil {
		fmt.Fprintf(os.Stderr, "WARN: picotool reboot returned an error, continuing to wait for serial: %v\n", rebootErr)
	}

	time.Sleep(cfg.postRebootGap)
	return waitForSerialPort(cfg)
}

func parseFlags() config {
	cfg := config{}
	flag.StringVar(&cfg.buildDir, "build-dir", defaultBuildDir(), "Directory containing test UF2 files")
	flag.StringVar(&cfg.picotool, "picotool", "picotool", "Path to picotool executable")
	flag.StringVar(&cfg.port, "port", "", "Serial device path; autodetect when empty")
	flag.StringVar(&cfg.pattern, "pattern", "sys_*.uf2", "Glob pattern for test UF2 files")
	flag.DurationVar(&cfg.timeout, "timeout", 15*time.Second, "Maximum time to wait for a test result")
	flag.DurationVar(&cfg.resultGrace, "result-grace", 500*time.Millisecond, "Extra time to keep reading serial output after the TEST PASS/FAIL line")
	flag.DurationVar(&cfg.loadTimeout, "load-timeout", 20*time.Second, "Maximum time for each picotool command")
	flag.IntVar(&cfg.baud, "baud", 115200, "Serial baud rate")
	flag.DurationVar(&cfg.pollInterval, "poll-interval", 250*time.Millisecond, "Polling interval for serial device detection")
	flag.DurationVar(&cfg.postRebootGap, "post-reboot-gap", 750*time.Millisecond, "Extra delay after reboot before probing serial")
	flag.Parse()
	return cfg
}

func defaultBuildDir() string {
	candidates := []string{
		filepath.Join("build", "test"),
		filepath.Join("..", "..", "build", "test"),
	}

	for _, candidate := range candidates {
		if info, err := os.Stat(candidate); err == nil && info.IsDir() {
			return candidate
		}
	}

	return candidates[0]
}

func discoverUF2s(buildDir, pattern string) ([]string, error) {
	matches, err := filepath.Glob(filepath.Join(buildDir, pattern))
	if err != nil {
		return nil, err
	}
	sort.Strings(matches)
	return matches, nil
}

func loadUF2(cfg config, uf2 string) error {
	return runCommand(cfg.loadTimeout, cfg.picotool, "load", "-f", "-v", uf2)
}

func rebootApplication(cfg config) error {
	return runCommand(cfg.loadTimeout, cfg.picotool, "reboot", "-a", "-f")
}

func runCommand(timeout time.Duration, name string, args ...string) error {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	cmd := exec.CommandContext(ctx, name, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		if ctx.Err() == context.DeadlineExceeded {
			return fmt.Errorf("command timed out: %s %s", name, strings.Join(args, " "))
		}
		return err
	}
	return nil
}

func waitForSerialPort(cfg config) (string, error) {
	if cfg.port != "" {
		return waitForNamedPort(cfg.port, cfg.timeout, cfg.pollInterval)
	}

	patterns := serialDevicePatterns()
	deadline := time.Now().Add(cfg.timeout)
	for time.Now().Before(deadline) {
		for _, pattern := range patterns {
			matches, err := filepath.Glob(pattern)
			if err != nil {
				return "", err
			}
			sort.Strings(matches)
			if len(matches) > 0 {
				return matches[0], nil
			}
		}
		time.Sleep(cfg.pollInterval)
	}
	return "", fmt.Errorf("timed out waiting for serial device matching %v", patterns)
}

func waitForNamedPort(port string, timeout, poll time.Duration) (string, error) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(port); err == nil {
			return port, nil
		}
		time.Sleep(poll)
	}
	return "", fmt.Errorf("timed out waiting for serial device %s", port)
}

func serialDevicePatterns() []string {
	switch runtime.GOOS {
	case "darwin":
		return []string{"/dev/cu.usbmodem*", "/dev/tty.usbmodem*", "/dev/cu.usbserial*"}
	case "linux":
		return []string{"/dev/ttyACM*", "/dev/ttyUSB*"}
	default:
		return []string{"/dev/ttyACM*", "/dev/ttyUSB*", "/dev/cu.usbmodem*"}
	}
}

func waitForResult(cfg config, portPath string) (testResult, error) {
	if err := configureSerialPort(portPath, cfg.baud); err != nil {
		return testResult{}, err
	}

	file, err := os.OpenFile(portPath, os.O_RDWR, 0)
	if err != nil {
		return testResult{}, err
	}
	defer file.Close()

	deadline := time.Now().Add(cfg.timeout)
	reader := bufio.NewReader(file)
	var result *testResult
	for time.Now().Before(deadline) {
		line, err := readLineWithDeadline(reader, file, deadline)
		if errors.Is(err, context.DeadlineExceeded) {
			break
		}
		if err != nil {
			return testResult{}, err
		}
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		fmt.Printf("    serial: %s\n", line)
		if matches := resultPattern.FindStringSubmatch(line); len(matches) == 4 {
			captured := testResult{
				status:    matches[1],
				name:      matches[2],
				elapsedMs: matches[3],
				line:      line,
			}
			result = &captured

			graceDeadline := time.Now().Add(cfg.resultGrace)
			if graceDeadline.Before(deadline) {
				deadline = graceDeadline
			}
		}
	}

	if result != nil {
		return *result, nil
	}

	return testResult{}, fmt.Errorf("timeout waiting for TEST PASS/FAIL line on %s", portPath)
}

func readLineWithDeadline(reader *bufio.Reader, file *os.File, deadline time.Time) (string, error) {
	for {
		if err := file.SetReadDeadline(deadline); err != nil {
			return "", err
		}
		line, err := reader.ReadString('\n')
		if err == nil {
			return line, nil
		}
		if errors.Is(err, os.ErrDeadlineExceeded) {
			return "", context.DeadlineExceeded
		}
		if errors.Is(err, io.EOF) {
			if time.Now().After(deadline) {
				return "", context.DeadlineExceeded
			}
			continue
		}
		return "", err
	}
}

func configureSerialPort(portPath string, baud int) error {
	args := serialConfigArgs(portPath, baud)
	if len(args) == 0 {
		return nil
	}
	cmd := exec.Command(args[0], args[1:]...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func serialConfigArgs(portPath string, baud int) []string {
	speed := fmt.Sprintf("%d", baud)
	switch runtime.GOOS {
	case "darwin":
		return []string{"stty", "-f", portPath, speed, "raw", "clocal", "-echo"}
	case "linux":
		return []string{"stty", "-F", portPath, speed, "raw", "clocal", "-echo"}
	default:
		return nil
	}
}

func fail(err error) {
	fmt.Fprintln(os.Stderr, "ERROR:", err)
	os.Exit(1)
}
