# ADR-0002: Lock-Free SPSC Ring Buffer and Keyframe-Aligned Pre-Event Buffer

- **Status:** Accepted
- **Requirement(s):** R1 (30 s pre-event buffer), R2 (`< 100 ms` trigger-to-recording latency)

## Context

A body-worn camera must continuously buffer the last 30 seconds of video so that when an officer draws a weapon (`holster` trigger) or presses the record button, the events leading up to the trigger are preserved. If the `capture` thread (`SCHED_FIFO 80`) acquires a `std::mutex` shared with the `writer` thread (`SCHED_OTHER`), any disk stall in the writer can cause priority inversion or block the capture thread past a 33.3 ms frame deadline. Furthermore, H.264/H.265 streams can only begin decoding at an IDR keyframe.

## Decision

1. **Wait-Free / Lock-Free SPSC Queue (`SpscRingBuffer<T>`):** The `capture` thread pushes frames into a power-of-two capacity single-producer/single-consumer ring buffer using `std::atomic` acquire/release indices with cache-line padding (`alignas(64)`). No mutexes or heap allocations occur on the capture path.
2. **Keyframe-Aligned `PreEventBuffer`:** Frames popped from the SPSC ring while in `Buffering` state are held in a bounded circular window pruned by timestamp (`now_ms - frame.ts_ms <= pre_event_ms`). When a trigger fires, `PreEventBuffer::drain()` locates the earliest frame in the window with `keyframe == true` and drains from that frame forward into the new incident's first segment.

## Alternatives Considered

1. **`std::mutex` + `std::deque<Frame>` shared between capture and writer:** Simple, but exposes the real-time capture thread to allocator jitter and lock contention with a lower-priority thread.
2. **Continuously writing pre-event segments to flash storage and deleting old files:** Causes severe eMMC/NAND write amplification (24/7 continuous writes) that wears out flash cells within months and consumes extra battery power.

## Consequences

- **Positive:** Zero flash wear while buffering in RAM; deterministic $O(1)$ capture path; every incident begins cleanly on a keyframe.
- **Negative:** Pre-event window consumes bounded RAM proportional to bitrate $\times$ `pre_event_ms`.
