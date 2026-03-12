--TEST--
PriorityFrame decodes exclusive, dependency, and weight
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$payload = pack('N', 0x80000003) . chr(10);
$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::PRIORITY, 0, 1, $payload));
$frame = $decoder->nextFrame();

echo get_class($frame), "\n";
var_dump($frame->isExclusive());
echo $frame->getStreamDependency(), "\n";
echo $frame->getWeight(), "\n";
?>
--EXPECT--
Varion\Http2\PriorityFrame
bool(true)
3
10
