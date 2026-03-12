--TEST--
RstStreamFrame decodes error code
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::RST_STREAM, 0, 1, pack('N', 2)));
$frame = $decoder->nextFrame();

echo get_class($frame), "\n";
echo $frame->getErrorCode(), "\n";
?>
--EXPECT--
Varion\Http2\RstStreamFrame
2
