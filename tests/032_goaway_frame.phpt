--TEST--
GoawayFrame decodes last stream id, error code, and debug data
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$payload = pack('N', 5) . pack('N', 2) . "bye";
$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::GOAWAY, 0, 0, $payload));
$frame = $decoder->nextFrame();

echo $frame->getLastStreamId(), "\n";
echo $frame->getErrorCode(), "\n";
echo $frame->getDebugData(), "\n";
?>
--EXPECT--
5
2
bye
