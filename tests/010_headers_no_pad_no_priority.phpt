--TEST--
HeadersFrame parses payload without PADDED/PRIORITY
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Flag;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::HEADERS, Flag::END_HEADERS, 1, "hdr"));

$frame = $decoder->nextFrame();
echo get_class($frame), "\n";
var_dump($frame->getPadLength());
var_dump($frame->hasPriority());
var_dump($frame->isExclusive());
var_dump($frame->getStreamDependency());
var_dump($frame->getWeight());
echo $frame->getHeaderBlockFragment(), "\n";
?>
--EXPECT--
Varion\Http2\HeadersFrame
NULL
bool(false)
bool(false)
NULL
NULL
hdr
