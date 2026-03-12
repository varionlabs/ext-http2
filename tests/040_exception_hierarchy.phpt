--TEST--
Exception classes are registered with expected inheritance
--EXTENSIONS--
http2
--FILE--
<?php

var_dump(class_exists('Varion\\Http2\\Exception'));
var_dump(class_exists('Varion\\Http2\\FrameError'));
var_dump(class_exists('Varion\\Http2\\PayloadLengthError'));
var_dump(class_exists('Varion\\Http2\\StreamStateError'));

var_dump(is_subclass_of('Varion\\Http2\\FrameError', 'Varion\\Http2\\Exception'));
var_dump(is_subclass_of('Varion\\Http2\\PayloadLengthError', 'Varion\\Http2\\FrameError'));
var_dump(is_subclass_of('Varion\\Http2\\StreamStateError', 'Varion\\Http2\\FrameError'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
