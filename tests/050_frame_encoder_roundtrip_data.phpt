--TEST--
FrameEncoder round-trips decoded DATA frame
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Flag;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameEncoder;
use Varion\Http2\FrameType;

$wire = h2_frame(FrameType::DATA, Flag::END_STREAM, 1, "abc");

$decoder = new FrameDecoder();
$decoder->push($wire);
$frame = $decoder->nextFrame();

$encoder = new FrameEncoder();
var_dump($encoder->encode($frame) === $wire);
?>
--EXPECT--
bool(true)
