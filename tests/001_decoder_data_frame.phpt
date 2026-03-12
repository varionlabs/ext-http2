--TEST--
FrameDecoder decodes one DATA frame
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::DATA, 0x1, 1, "abc"));

$frame = $decoder->nextFrame();
echo get_class($frame), "\n";
echo $frame->getType(), ":", $frame->getFlags(), ":", $frame->getStreamId(), ":", $frame->getPayloadLength(), ":", $frame->getPayload(), "\n";
var_dump($decoder->nextFrame());
?>
--EXPECT--
Varion\Http2\DataFrame
0:1:1:3:abc
NULL
