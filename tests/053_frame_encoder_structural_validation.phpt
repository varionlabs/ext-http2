--TEST--
FrameEncoder applies decoder-equivalent structural validation rules
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Frame;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameEncoder;
use Varion\Http2\FrameType;

$payloadProp = new ReflectionProperty(Frame::class, 'payload');

$encoder = new FrameEncoder();

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::PING, 0, 0, "12345678"));
$ping = $decoder->nextFrame();
$payloadProp->setValue($ping, "1234567");

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::WINDOW_UPDATE, 0, 1, pack('N', 1)));
$window = $decoder->nextFrame();
$payloadProp->setValue($window, pack('N', 0));

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::SETTINGS, 0, 0, pack('nN', 1, 100)));
$settings = $decoder->nextFrame();
$payloadProp->setValue($settings, "12345");

foreach ([$ping, $window, $settings] as $frame) {
    try {
        $encoder->encode($frame);
        echo "no-error\n";
    } catch (Throwable $e) {
        echo get_class($e), "\n";
    }
}
?>
--EXPECT--
Varion\Http2\PayloadLengthError
Varion\Http2\PayloadLengthError
Varion\Http2\PayloadLengthError
