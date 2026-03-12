PHP_ARG_ENABLE([http2],
  [whether to enable http2 extension],
  [AS_HELP_STRING([--enable-http2], [Enable http2 extension])],
  [no])

if test "$PHP_HTTP2" != "no"; then
  PHP_NEW_EXTENSION([http2], [http2.c src/frame.c src/frame_decoder.c src/headers_block_assembler.c], [$ext_shared])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
fi
