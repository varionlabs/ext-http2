--TEST--
PingFrame exposes opaque 8-byte payload
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::PING, 0, 0, "12345678"));
$frame = $decoder->nextFrame();

echo get_class($frame), "\n";
echo $frame->getOpaqueData(), "\n";
?>
--EXPECT--
Varion\Http2\PingFrame
12345678
