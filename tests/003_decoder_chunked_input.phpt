--TEST--
FrameDecoder handles chunked input
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$bytes = h2_frame(FrameType::DATA, 0, 1, "hello");
$decoder = new FrameDecoder();

$decoder->push(substr($bytes, 0, 4));
var_dump($decoder->nextFrame());

echo $decoder->getBufferedLength(), "\n";
$decoder->push(substr($bytes, 4));
$frame = $decoder->nextFrame();

echo get_class($frame), "\n";
echo $frame->getPayload(), "\n";
?>
--EXPECT--
NULL
4
Varion\Http2\DataFrame
hello
