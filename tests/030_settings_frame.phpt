--TEST--
SettingsFrame decodes settings map
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$payload = pack('nNnN', 1, 100, 4, 65535);
$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::SETTINGS, 0, 0, $payload));
$frame = $decoder->nextFrame();

var_export($frame->getSettings());
echo "\n";
?>
--EXPECT--
array (
  1 => 100,
  4 => 65535,
)
