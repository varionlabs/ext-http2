#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_http2.h"

zend_class_entry *http2_ce_event;
zend_class_entry *http2_ce_frame_decoded_event;
zend_class_entry *http2_ce_headers_block_completed_event;
zend_class_entry *http2_ce_decoder_error_event;
zend_class_entry *http2_ce_event_factory;

PHP_METHOD(Http2EventFactory, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(Http2EventFactory, frameDecoded)
{
    zval *frame;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(frame, http2_ce_frame)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, http2_ce_frame_decoded_event);
    zend_update_property(http2_ce_frame_decoded_event, Z_OBJ_P(return_value), ZEND_STRL("frame"), frame);
}

PHP_METHOD(Http2EventFactory, headersBlockCompleted)
{
    zend_long stream_id;
    zend_string *header_block_fragment;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(stream_id)
        Z_PARAM_STR(header_block_fragment)
    ZEND_PARSE_PARAMETERS_END();

    if (stream_id < 0 || stream_id > 0x7FFFFFFF) {
        zend_throw_exception_ex(http2_ce_frame_error_exception, 0, "streamId must be in range 0..2147483647, got %ld", stream_id);
        RETURN_THROWS();
    }

    object_init_ex(return_value, http2_ce_headers_block_completed_event);
    zend_update_property_long(
        http2_ce_headers_block_completed_event,
        Z_OBJ_P(return_value),
        ZEND_STRL("streamId"),
        stream_id
    );
    zend_update_property_stringl(
        http2_ce_headers_block_completed_event,
        Z_OBJ_P(return_value),
        ZEND_STRL("headerBlockFragment"),
        ZSTR_VAL(header_block_fragment),
        ZSTR_LEN(header_block_fragment)
    );
}

PHP_METHOD(Http2EventFactory, decoderError)
{
    zend_string *message;
    zend_long code = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(message)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(code)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, http2_ce_decoder_error_event);
    zend_update_property_stringl(
        http2_ce_decoder_error_event,
        Z_OBJ_P(return_value),
        ZEND_STRL("message"),
        ZSTR_VAL(message),
        ZSTR_LEN(message)
    );
    zend_update_property_long(http2_ce_decoder_error_event, Z_OBJ_P(return_value), ZEND_STRL("code"), code);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_http2_event_factory_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_http2_event_factory_frame_decoded, 0, 1, Varion\\Http2\\FrameDecoded, 0)
    ZEND_ARG_OBJ_INFO(0, frame, Varion\\Http2\\Frame, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_http2_event_factory_headers_block_completed, 0, 2, Varion\\Http2\\HeadersBlockCompleted, 0)
    ZEND_ARG_TYPE_INFO(0, streamId, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, headerBlockFragment, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_http2_event_factory_decoder_error, 0, 1, Varion\\Http2\\DecoderError, 0)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, code, IS_LONG, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry http2_event_factory_methods[] = {
    PHP_ME(Http2EventFactory, __construct, arginfo_http2_event_factory_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Http2EventFactory, frameDecoded, arginfo_http2_event_factory_frame_decoded, ZEND_ACC_PUBLIC)
    PHP_ME(Http2EventFactory, headersBlockCompleted, arginfo_http2_event_factory_headers_block_completed, ZEND_ACC_PUBLIC)
    PHP_ME(Http2EventFactory, decoderError, arginfo_http2_event_factory_decoder_error, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void http2_register_event_classes(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "Event", NULL);
    http2_ce_event = zend_register_internal_class(&ce);
    http2_ce_event->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "FrameDecoded", NULL);
    http2_ce_frame_decoded_event = zend_register_internal_class_ex(&ce, http2_ce_event);
    zend_declare_property_null(http2_ce_frame_decoded_event, ZEND_STRL("frame"), ZEND_ACC_PUBLIC);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "HeadersBlockCompleted", NULL);
    http2_ce_headers_block_completed_event = zend_register_internal_class_ex(&ce, http2_ce_event);
    zend_declare_property_long(http2_ce_headers_block_completed_event, ZEND_STRL("streamId"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(http2_ce_headers_block_completed_event, ZEND_STRL("headerBlockFragment"), "", ZEND_ACC_PUBLIC);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "DecoderError", NULL);
    http2_ce_decoder_error_event = zend_register_internal_class_ex(&ce, http2_ce_event);
    zend_declare_property_string(http2_ce_decoder_error_event, ZEND_STRL("message"), "", ZEND_ACC_PUBLIC);
    zend_declare_property_long(http2_ce_decoder_error_event, ZEND_STRL("code"), 0, ZEND_ACC_PUBLIC);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "EventFactory", http2_event_factory_methods);
    http2_ce_event_factory = zend_register_internal_class(&ce);
}
