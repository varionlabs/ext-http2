--TEST--
FrameEncoder throws FrameError for frame with missing payload representation
--EXTENSIONS--
http2
--FILE--
<?php

use Varion\Http2\DataFrame;
use Varion\Http2\FrameEncoder;

$encoder = new FrameEncoder();
$frame = new DataFrame();

try {
    $encoder->encode($frame);
    echo "no-error\n";
} catch (Throwable $e) {
    echo get_class($e), "\n";
}
?>
--EXPECT--
Varion\Http2\FrameError
