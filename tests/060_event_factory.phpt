--TEST--
EventFactory creates logic-free notification objects
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Event;
use Varion\Http2\EventFactory;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;

$decoder = new FrameDecoder();
$decoder->push(h2_frame(FrameType::DATA, 0, 1, "x"));
$frame = $decoder->nextFrame();

$factory = new EventFactory();
$frameEvent = $factory->frameDecoded($frame);

$headersEvent = $factory->headersBlockCompleted(3, "abc");
$errorEvent = $factory->decoderError("broken", 7);

echo get_class($frameEvent), ":", get_class($frameEvent->frame), "\n";
echo get_class($headersEvent), ":", $headersEvent->streamId, ":", $headersEvent->headerBlockFragment, "\n";
echo get_class($errorEvent), ":", $errorEvent->message, ":", $errorEvent->code, "\n";
var_dump($frameEvent instanceof Event);
var_dump($headersEvent instanceof Event);
var_dump($errorEvent instanceof Event);
?>
--EXPECT--
Varion\Http2\FrameDecoded:Varion\Http2\DataFrame
Varion\Http2\HeadersBlockCompleted:3:abc
Varion\Http2\DecoderError:broken:7
bool(true)
bool(true)
bool(true)
