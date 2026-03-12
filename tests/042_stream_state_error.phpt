--TEST--
Invalid header assembly sequence throws StreamStateError
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;
use Varion\Http2\HeadersBlockAssembler;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::CONTINUATION, 0, 1, "x"));
$frame = $decoder->nextFrame();

$assembler = new HeadersBlockAssembler();

try {
    $assembler->push($frame);
    echo "no-error\n";
} catch (Throwable $e) {
    echo get_class($e), "\n";
}
?>
--EXPECT--
Varion\Http2\StreamStateError
