--TEST--
FrameEncoder round-trips multiple frame types
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameEncoder;
use Varion\Http2\FrameType;

$wire = h2_frame(FrameType::PRIORITY, 0, 1, pack('N', 0x80000003) . chr(10))
    . h2_frame(FrameType::PING, 0, 0, "12345678")
    . h2_frame(FrameType::GOAWAY, 0, 0, pack('N', 5) . pack('N', 2) . "bye");

$decoder = new FrameDecoder();
$decoder->push($wire);
$frames = $decoder->drain();

$encoder = new FrameEncoder();
$out = '';
foreach ($frames as $frame) {
    $out .= $encoder->encode($frame);
}

var_dump($out === $wire);
?>
--EXPECT--
bool(true)
