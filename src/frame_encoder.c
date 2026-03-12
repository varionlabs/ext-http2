#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_http2.h"

zend_class_entry *http2_ce_frame_encoder;

static zval *http2_encoder_read_frame_property(zval *frame, const char *name, size_t name_len)
{
    return zend_read_property(http2_ce_frame, Z_OBJ_P(frame), name, name_len, 1, NULL);
}

PHP_METHOD(Http2FrameEncoder, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(Http2FrameEncoder, encode)
{
    zval *frame;
    zval *type_prop;
    zval *flags_prop;
    zval *stream_id_prop;
    zval *payload_prop;
    zend_long type;
    zend_long flags;
    zend_long stream_id;
    size_t payload_length;
    zend_string *result;
    unsigned char *out;
    zval validated_frame;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(frame, http2_ce_frame)
    ZEND_PARSE_PARAMETERS_END();

    type_prop = http2_encoder_read_frame_property(frame, ZEND_STRL("type"));
    flags_prop = http2_encoder_read_frame_property(frame, ZEND_STRL("flags"));
    stream_id_prop = http2_encoder_read_frame_property(frame, ZEND_STRL("streamId"));
    payload_prop = http2_encoder_read_frame_property(frame, ZEND_STRL("payload"));

    type = zval_get_long(type_prop);
    flags = zval_get_long(flags_prop);
    stream_id = zval_get_long(stream_id_prop);

    if (type < 0 || type > 0xFF) {
        zend_throw_exception_ex(http2_ce_frame_error_exception, 0, "Frame type must be in range 0..255, got %ld", type);
        RETURN_THROWS();
    }

    if (flags < 0 || flags > 0xFF) {
        zend_throw_exception_ex(http2_ce_frame_error_exception, 0, "Frame flags must be in range 0..255, got %ld", flags);
        RETURN_THROWS();
    }

    if (stream_id < 0 || stream_id > 0x7FFFFFFF) {
        zend_throw_exception_ex(http2_ce_frame_error_exception, 0, "Frame streamId must be in range 0..2147483647, got %ld", stream_id);
        RETURN_THROWS();
    }

    if (Z_TYPE_P(payload_prop) != IS_STRING) {
        zend_throw_exception_ex(
            http2_ce_frame_error_exception,
            0,
            "Frame payload must be a string; encode currently expects decoded frame objects"
        );
        RETURN_THROWS();
    }

    payload_length = Z_STRLEN_P(payload_prop);
    if (payload_length > HTTP2_MAX_ALLOWED_FRAME_SIZE) {
        zend_throw_exception_ex(
            http2_ce_payload_length_error_exception,
            0,
            "Frame payload length %zu exceeds max allowed size %u",
            payload_length,
            (unsigned int) HTTP2_MAX_ALLOWED_FRAME_SIZE
        );
        RETURN_THROWS();
    }

    /* Reuse decoder-side frame construction to enforce identical structural validation rules. */
    ZVAL_UNDEF(&validated_frame);
    if (http2_create_frame_object(
        (uint8_t) type,
        (uint8_t) flags,
        (uint32_t) stream_id,
        (const unsigned char *) Z_STRVAL_P(payload_prop),
        payload_length,
        &validated_frame
    ) != SUCCESS) {
        RETURN_THROWS();
    }
    zval_ptr_dtor(&validated_frame);

    result = zend_string_alloc(9 + payload_length, 0);
    out = (unsigned char *) ZSTR_VAL(result);

    out[0] = (unsigned char) ((payload_length >> 16) & 0xFF);
    out[1] = (unsigned char) ((payload_length >> 8) & 0xFF);
    out[2] = (unsigned char) (payload_length & 0xFF);
    out[3] = (unsigned char) type;
    out[4] = (unsigned char) flags;
    out[5] = (unsigned char) (((uint32_t) stream_id >> 24) & 0xFF);
    out[6] = (unsigned char) (((uint32_t) stream_id >> 16) & 0xFF);
    out[7] = (unsigned char) (((uint32_t) stream_id >> 8) & 0xFF);
    out[8] = (unsigned char) ((uint32_t) stream_id & 0xFF);

    if (payload_length > 0) {
        memcpy(out + 9, Z_STRVAL_P(payload_prop), payload_length);
    }

    out[9 + payload_length] = '\0';

    RETURN_NEW_STR(result);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_http2_encoder_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_encoder_encode, 0, 1, IS_STRING, 0)
    ZEND_ARG_OBJ_INFO(0, frame, Varion\\Http2\\Frame, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry http2_frame_encoder_methods[] = {
    PHP_ME(Http2FrameEncoder, __construct, arginfo_http2_encoder_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameEncoder, encode, arginfo_http2_encoder_encode, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void http2_register_frame_encoder_class(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "FrameEncoder", http2_frame_encoder_methods);
    http2_ce_frame_encoder = zend_register_internal_class(&ce);
}
