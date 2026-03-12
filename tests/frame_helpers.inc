<?php

function h2_frame(int $type, int $flags, int $streamId, string $payload): string
{
    $len = strlen($payload);
    return chr(($len >> 16) & 0xff)
        . chr(($len >> 8) & 0xff)
        . chr($len & 0xff)
        . chr($type & 0xff)
        . chr($flags & 0xff)
        . pack('N', $streamId & 0x7fffffff)
        . $payload;
}
