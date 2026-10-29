package ipc

import (
	"bytes"
	"io"
	"strings"
	"testing"
)

type duplexBuf struct {
	in  *bytes.Buffer
	out *bytes.Buffer
}

func (d *duplexBuf) Read(p []byte) (int, error)  { return d.in.Read(p) }
func (d *duplexBuf) Write(p []byte) (int, error) { return d.out.Write(p) }

var _ io.ReadWriter = (*duplexBuf)(nil)

func TestClientCommandsAndInterleavedEvents(t *testing.T) {
	serverReplies := strings.Join([]string{
		`EVT {"type":"segment_sealed","incident":"INC-AEG-0001-10","index":1,"path":"seg_000001.bin","sha256":"abcd"}`,
		`OK {"state":"Recording","net":"up"}`,
		`OK {"acked":1}`,
		`ERR INVALID_ARGUMENT bad_segment`,
	}, "\n") + "\n"

	rw := &duplexBuf{
		in:  bytes.NewBufferString(serverReplies),
		out: &bytes.Buffer{},
	}
	cli := NewClient(rw)

	resp, err := cli.SendNet(true)
	if err != nil || resp["net"] != "up" {
		t.Fatalf("SendNet failed: resp=%v err=%v", resp, err)
	}

	evts := cli.DrainEvents()
	if len(evts) != 1 || evts[0].Type != "segment_sealed" || evts[0].Index != 1 {
		t.Fatalf("unexpected drained events: %+v", evts)
	}

	if _, err := cli.SendUploadAck("INC-AEG-0001-10", 1); err != nil {
		t.Fatalf("SendUploadAck: %v", err)
	}

	if _, err := cli.Command("UPLOAD_ACK INC-AEG-0001-10 99"); err == nil {
		t.Fatal("expected error for ERR response")
	}

	sent := rw.out.String()
	wantSent := "NET up\nUPLOAD_ACK INC-AEG-0001-10 1\nUPLOAD_ACK INC-AEG-0001-10 99\n"
	if sent != wantSent {
		t.Fatalf("sent=%q, want %q", sent, wantSent)
	}
}
