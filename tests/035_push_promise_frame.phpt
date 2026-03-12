--TEST--
PushPromiseFrame decodes promised stream id and fragment with padding
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Flag;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$payload = chr(1) . pack('N', 7) . "abc" . "\0";
$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::PUSH_PROMISE, Flag::PADDED | Flag::END_HEADERS, 1, $payload));
$frame = $decoder->nextFrame();

echo get_class($frame), "\n";
echo $frame->getPadLength(), "\n";
echo $frame->getPromisedStreamId(), "\n";
echo $frame->getHeaderBlockFragment(), "\n";
?>
--EXPECT--
Varion\Http2\PushPromiseFrame
1
7
abc
