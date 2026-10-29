// Package ipc implements the line-protocol client used by netagent to report
// link state (`NET up|down`), acknowledge uploaded segments (`UPLOAD_ACK`),
// and receive asynchronous `EVT {...}` notifications from aegisd.
package ipc

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
	"sync"
)

// Event represents an asynchronous `EVT {json}` notification from aegisd.
type Event struct {
	Type     string `json:"type"`
	Incident string `json:"incident"`
	Index    int    `json:"index,omitempty"`
	Path     string `json:"path,omitempty"`
	SHA256   string `json:"sha256,omitempty"`
	Segments int    `json:"segments,omitempty"`
}

// Client speaks the aegisd §5 ASCII line protocol over an io.ReadWriter.
type Client struct {
	mu     sync.Mutex
	r      *bufio.Reader
	w      io.Writer
	events []Event
}

// NewClient wraps rw in a protocol Client.
func NewClient(rw io.ReadWriter) *Client {
	return &Client{
		r: bufio.NewReader(rw),
		w: rw,
	}
}

// SendNet reports link state ("up" or "down") to aegisd.
func (c *Client) SendNet(up bool) (map[string]any, error) {
	state := "down"
	if up {
		state = "up"
	}
	return c.Command(fmt.Sprintf("NET %s", state))
}

// SendUploadAck notifies aegisd that a segment has been safely stored in cloud.
func (c *Client) SendUploadAck(incidentID string, segmentIndex int) (map[string]any, error) {
	if strings.TrimSpace(incidentID) == "" || segmentIndex <= 0 {
		return nil, errors.New("ipc: invalid incidentID or segmentIndex")
	}
	return c.Command(fmt.Sprintf("UPLOAD_ACK %s %d", incidentID, segmentIndex))
}

// Command sends one line command, buffers any interleaved `EVT ` lines, and
// returns the parsed JSON payload of an `OK {json}` response.
func (c *Client) Command(cmd string) (map[string]any, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if strings.ContainsAny(cmd, "\r\n") {
		return nil, errors.New("ipc: command contains newline")
	}
	if _, err := fmt.Fprintf(c.w, "%s\n", cmd); err != nil {
		return nil, err
	}

	for {
		line, err := c.r.ReadString('\n')
		if err != nil {
			return nil, err
		}
		line = strings.TrimRight(line, "\r\n")
		if strings.HasPrefix(line, "EVT ") {
			var ev Event
			if err := json.Unmarshal([]byte(strings.TrimPrefix(line, "EVT ")), &ev); err == nil {
				c.events = append(c.events, ev)
			}
			continue
		}
		if strings.HasPrefix(line, "OK ") {
			var out map[string]any
			raw := strings.TrimPrefix(line, "OK ")
			if err := json.Unmarshal([]byte(raw), &out); err != nil {
				return nil, fmt.Errorf("ipc: invalid OK json %q: %w", raw, err)
			}
			return out, nil
		}
		if line == "OK" {
			return map[string]any{}, nil
		}
		if strings.HasPrefix(line, "ERR ") {
			return nil, fmt.Errorf("ipc: %s", line)
		}
		return nil, fmt.Errorf("ipc: unexpected response %q", line)
	}
}

// DrainEvents returns and clears all asynchronous events received so far.
func (c *Client) DrainEvents() []Event {
	c.mu.Lock()
	defer c.mu.Unlock()
	out := append([]Event(nil), c.events...)
	c.events = nil
	return out
}
