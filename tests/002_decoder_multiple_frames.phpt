--TEST--
FrameDecoder decodes multiple frames in one buffer
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$chunk = h2_frame(FrameType::DATA, 0, 1, "A")
    . h2_frame(FrameType::DATA, 1, 3, "BC");
$decoder->push($chunk);

$frames = $decoder->drain();

echo count($frames), "\n";
foreach ($frames as $frame) {
    echo $frame->getStreamId(), ":", $frame->getPayload(), ":", $frame->getFlags(), "\n";
}

echo $decoder->getBufferedLength(), "\n";
?>
--EXPECT--
2
1:A:0
3:BC:1
0
