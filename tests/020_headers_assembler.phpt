--TEST--
HeadersBlockAssembler joins HEADERS and CONTINUATION fragments
--EXTENSIONS--
http2
--FILE--
<?php
require __DIR__ . '/frame_helpers.inc';

use Varion\Http2\Flag;
use Varion\Http2\FrameDecoder;
use Varion\Http2\FrameType;
use Varion\Http2\HeadersBlockAssembler;

$decoder = new FrameDecoder();
$decoder->push(
    h2_frame(FrameType::HEADERS, 0, 3, "abc")
    . h2_frame(FrameType::CONTINUATION, Flag::END_HEADERS, 3, "def")
);

$frames = $decoder->drain();
$assembler = new HeadersBlockAssembler();

$assembler->push($frames[0]);
var_dump($assembler->isComplete());
$assembler->push($frames[1]);
var_dump($assembler->isComplete());
echo $assembler->getStreamId(), "\n";
echo $assembler->getHeaderBlockFragment(), "\n";
?>
--EXPECT--
bool(false)
bool(true)
3
abcdef
