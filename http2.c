#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_http2.h"

PHP_MINIT_FUNCTION(http2)
{
    http2_register_exception_classes();
    http2_register_http2_constants();
    http2_register_frame_classes();
    http2_register_frame_decoder_class();
    http2_register_frame_encoder_class();
    http2_register_headers_block_assembler_class();
    http2_register_event_classes();

    return SUCCESS;
}

PHP_MINFO_FUNCTION(http2)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "http2 support", "enabled");
    php_info_print_table_row(2, "version", PHP_HTTP2_VERSION);
    php_info_print_table_end();
}

zend_module_entry http2_module_entry = {
    STANDARD_MODULE_HEADER,
    "http2",
    NULL,
    PHP_MINIT(http2),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(http2),
    PHP_HTTP2_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HTTP2
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(http2)
#endif
