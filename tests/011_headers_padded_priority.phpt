--TEST--
HeadersFrame parses PADDED and PRIORITY fields
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Flag;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$payload = chr(2) . pack('N', 0x80000003) . chr(15) . "xyz" . "\0\0";
$flags = Flag::END_HEADERS | Flag::PADDED | Flag::PRIORITY;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::HEADERS, $flags, 1, $payload));
$frame = $decoder->nextFrame();

echo $frame->getPadLength(), "\n";
var_dump($frame->hasPriority());
var_dump($frame->isExclusive());
echo $frame->getStreamDependency(), "\n";
echo $frame->getWeight(), "\n";
echo $frame->getHeaderBlockFragment(), "\n";
?>
--EXPECT--
2
bool(true)
bool(true)
3
15
xyz
