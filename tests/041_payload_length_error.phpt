--TEST--
Invalid payload lengths throw PayloadLengthError
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::PING, 0, 0, "1234567"));

try {
    $decoder->nextFrame();
    echo "no-error\n";
} catch (Throwable $e) {
    echo get_class($e), "\n";
}
?>
--EXPECT--
Varion\Http2\PayloadLengthError
