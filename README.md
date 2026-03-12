# ext-http2 (prototype)

`ext-http2` is a PHP extension that focuses on the HTTP/2 binary framing layer.
It is intentionally not a full HTTP/2 stack.

## Purpose

- Parse HTTP/2 frames from raw TCP byte streams.
- Expose typed frame objects to PHP.
- Provide HEADERS + CONTINUATION fragment assembly support.
- Offer a practical base for future integration with nghttp2-based or pure-PHP HPACK/session implementations.

## Scope (implemented)

- Namespace: `Varion\\Http2`
- Frame classes:
  - `Frame`
  - `DataFrame`
  - `HeadersFrame`
  - `PriorityFrame`
  - `RstStreamFrame`
  - `PushPromiseFrame`
  - `SettingsFrame`
  - `PingFrame`
  - `GoawayFrame`
  - `WindowUpdateFrame`
  - `ContinuationFrame`
  - `UnknownFrame`
- Incremental parser:
  - `FrameDecoder::push(string $chunk): void`
  - `FrameDecoder::nextFrame(): ?Frame`
  - `FrameDecoder::drain(): array`
  - `FrameDecoder::reset(): void`
  - `FrameDecoder::getBufferedLength(): int`
- Wire encoder:
  - `FrameEncoder::encode(Frame $frame): string`
- Header block assembly helper:
  - `HeadersBlockAssembler::push(Frame $frame): void`
  - `HeadersBlockAssembler::isComplete(): bool`
  - `HeadersBlockAssembler::getHeaderBlockFragment(): string`
  - `HeadersBlockAssembler::getStreamId(): int`
  - `HeadersBlockAssembler::reset(): void`
- Event notifications:
  - `Event` (abstract)
  - `FrameDecoded`
  - `HeadersBlockCompleted`
  - `DecoderError`
  - `EventFactory`
- Constants:
  - `FrameType::{DATA,HEADERS,PRIORITY,RST_STREAM,SETTINGS,PUSH_PROMISE,PING,GOAWAY,WINDOW_UPDATE,CONTINUATION}`
  - `Flag::{END_STREAM,ACK,END_HEADERS,PADDED,PRIORITY}`
- Exception classes:
  - `Exception`
  - `FrameError`
  - `PayloadLengthError`
  - `StreamStateError`

## Out of scope (not implemented)

- HPACK encoder/decoder.
- Dynamic table.
- Pseudo-header validation.
- Full stream/session state machine.
- Flow-control state machine.
- Full server/client implementation.
- Direct nghttp2 integration.

## Build

```bash
phpize
./configure --enable-http2
make
make test
```

## Quick usage example

```php
<?php
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

function h2_frame(int $type, int $flags, int $streamId, string $payload): string {
    $len = strlen($payload);
    return chr(($len >> 16) & 0xff)
        . chr(($len >> 8) & 0xff)
        . chr($len & 0xff)
        . chr($type)
        . chr($flags)
        . pack('N', $streamId & 0x7fffffff)
        . $payload;
}

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::DATA, 0x1, 1, "hello"));
$frame = $decoder->nextFrame();

echo $frame->getPayload(); // hello
```

## Header block note

`HeadersFrame::getHeaderBlockFragment()` and `HeadersBlockAssembler::getHeaderBlockFragment()` return HPACK-encoded raw binary fragments.
No HPACK decoding is performed in this prototype.

## Directory structure

```text
.
├── config.m4
├── http2.c
├── php_http2.h
├── src
│   ├── frame.c
│   ├── frame_decoder.c
│   ├── frame_encoder.c
│   ├── event.c
│   └── headers_block_assembler.c
└── tests
    ├── frame_helpers.inc
    ├── 001_decoder_data_frame.phpt
    ├── 002_decoder_multiple_frames.phpt
    ├── 003_decoder_chunked_input.phpt
    ├── 010_headers_no_pad_no_priority.phpt
    ├── 011_headers_padded_priority.phpt
    ├── 020_headers_assembler.phpt
    ├── 030_settings_frame.phpt
    ├── 031_ping_frame.phpt
    ├── 032_goaway_frame.phpt
    ├── 033_priority_frame.phpt
    ├── 034_rst_stream_frame.phpt
    ├── 035_push_promise_frame.phpt
    ├── 040_exception_hierarchy.phpt
    ├── 041_payload_length_error.phpt
    ├── 042_stream_state_error.phpt
    ├── 050_frame_encoder_roundtrip_data.phpt
    ├── 051_frame_encoder_roundtrip_multiple.phpt
    ├── 052_frame_encoder_invalid_input.phpt
    ├── 053_frame_encoder_structural_validation.phpt
    └── 060_event_factory.phpt
```

## Main classes and responsibilities

- `Frame` and subclasses:
  - Hold parsed frame metadata and binary payload.
  - Expose typed getters for structured fields.
- `FrameDecoder`:
  - Incrementally buffers bytes and emits complete frames.
  - Handles fragmented TCP input.
  - Applies a payload-size guard (`HTTP2_MAX_ALLOWED_FRAME_SIZE = 1 MiB`).
- `FrameEncoder`:
  - Converts frame representation objects back into HTTP/2 wire bytes.
  - Reuses the frame metadata (`type`, `flags`, `streamId`, `payload`) for serialization.
  - Applies the same structural checks used by decoder-side frame parsing.
- `HeadersBlockAssembler`:
  - Reconstructs a single header block from `HEADERS` + `CONTINUATION` on one stream.
  - Detects invalid stream-id transitions.
- `EventFactory` and event objects:
  - `EventFactory` creates notification objects.
  - Event classes are data carriers only and contain no behavior.

## Current constraints

- Frame payload hard-limit is 1 MiB in this prototype.
- One `HeadersBlockAssembler` instance assembles one block sequence at a time.
- Error code/settings values that exceed platform `zend_long` on 32-bit builds are rejected with `FrameError`.

## Memory-management notes

- Decoder buffer grows with `push()` and is compacted when consumption advances.
- Frame payloads and assembled fragments are stored as owned `zend_string` data.
- Custom objects (`FrameDecoder`, `HeadersBlockAssembler`) release internal buffers in their object destructors.

## Future extension ideas

- Protocol error classification helpers.
- nghttp2 inflater adapter for HPACK decode handoff.
