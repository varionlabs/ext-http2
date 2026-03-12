#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_http2.h"

typedef struct _http2_frame_decoder_object {
    zend_string *buffer;
    size_t offset;
    zend_object std;
} http2_frame_decoder_object;

static zend_object_handlers http2_frame_decoder_handlers;

static inline http2_frame_decoder_object *http2_frame_decoder_fetch_object(zend_object *obj)
{
    return (http2_frame_decoder_object *) ((char *) obj - XtOffsetOf(http2_frame_decoder_object, std));
}

#define Z_HTTP2_FRAME_DECODER_OBJ_P(zv) http2_frame_decoder_fetch_object(Z_OBJ_P((zv)))

zend_class_entry *http2_ce_frame_decoder;

static uint32_t http2_decoder_read_u32be(const unsigned char *p)
{
    return ((uint32_t) p[0] << 24)
        | ((uint32_t) p[1] << 16)
        | ((uint32_t) p[2] << 8)
        | (uint32_t) p[3];
}

static void http2_frame_decoder_compact_buffer(http2_frame_decoder_object *decoder)
{
    size_t length;
    size_t remaining;

    if (decoder->buffer == NULL || decoder->offset == 0) {
        return;
    }

    length = ZSTR_LEN(decoder->buffer);

    if (decoder->offset >= length) {
        zend_string_release(decoder->buffer);
        decoder->buffer = NULL;
        decoder->offset = 0;
        return;
    }

    if (decoder->offset < 4096 && decoder->offset < (length / 2)) {
        return;
    }

    remaining = length - decoder->offset;
    memmove(ZSTR_VAL(decoder->buffer), ZSTR_VAL(decoder->buffer) + decoder->offset, remaining);
    decoder->buffer = zend_string_truncate(decoder->buffer, remaining, 0);
    ZSTR_VAL(decoder->buffer)[remaining] = '\0';
    decoder->offset = 0;
}

static void http2_frame_decoder_append(http2_frame_decoder_object *decoder, zend_string *chunk)
{
    size_t current_length;
    size_t chunk_length = ZSTR_LEN(chunk);

    if (chunk_length == 0) {
        return;
    }

    if (decoder->buffer == NULL) {
        decoder->buffer = zend_string_init(ZSTR_VAL(chunk), chunk_length, 0);
        decoder->offset = 0;
        return;
    }

    current_length = ZSTR_LEN(decoder->buffer);
    decoder->buffer = zend_string_extend(decoder->buffer, current_length + chunk_length, 0);
    memcpy(ZSTR_VAL(decoder->buffer) + current_length, ZSTR_VAL(chunk), chunk_length);
    ZSTR_VAL(decoder->buffer)[current_length + chunk_length] = '\0';
}

static int http2_frame_decoder_next_internal(http2_frame_decoder_object *decoder, zval *frame)
{
    const unsigned char *cursor;
    size_t buffered_length;
    size_t available;
    size_t frame_total_length;
    uint32_t payload_length;
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id;

    if (decoder->buffer == NULL) {
        return 0;
    }

    buffered_length = ZSTR_LEN(decoder->buffer);
    if (decoder->offset >= buffered_length) {
        http2_frame_decoder_compact_buffer(decoder);
        return 0;
    }

    available = buffered_length - decoder->offset;
    if (available < 9) {
        return 0;
    }

    cursor = (const unsigned char *) ZSTR_VAL(decoder->buffer) + decoder->offset;
    payload_length = ((uint32_t) cursor[0] << 16) | ((uint32_t) cursor[1] << 8) | (uint32_t) cursor[2];

    if (payload_length > HTTP2_MAX_ALLOWED_FRAME_SIZE) {
        zend_throw_exception_ex(
            http2_ce_payload_length_error_exception,
            0,
            "Frame payload length %u exceeds max allowed size %u",
            payload_length,
            (unsigned int) HTTP2_MAX_ALLOWED_FRAME_SIZE
        );
        return -1;
    }

    frame_total_length = (size_t) payload_length + 9;
    if (available < frame_total_length) {
        return 0;
    }

    type = cursor[3];
    flags = cursor[4];
    stream_id = http2_decoder_read_u32be(cursor + 5) & 0x7FFFFFFFU;

    if (http2_create_frame_object(
        type,
        flags,
        stream_id,
        cursor + 9,
        payload_length,
        frame
    ) != SUCCESS) {
        return -1;
    }

    decoder->offset += frame_total_length;
    http2_frame_decoder_compact_buffer(decoder);

    return 1;
}

PHP_METHOD(Http2FrameDecoder, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(Http2FrameDecoder, push)
{
    zend_string *chunk;
    http2_frame_decoder_object *decoder = Z_HTTP2_FRAME_DECODER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(chunk)
    ZEND_PARSE_PARAMETERS_END();

    http2_frame_decoder_append(decoder, chunk);
}

PHP_METHOD(Http2FrameDecoder, nextFrame)
{
    http2_frame_decoder_object *decoder = Z_HTTP2_FRAME_DECODER_OBJ_P(ZEND_THIS);
    zval frame;
    int status;

    ZEND_PARSE_PARAMETERS_NONE();

    ZVAL_UNDEF(&frame);
    status = http2_frame_decoder_next_internal(decoder, &frame);

    if (status < 0) {
        RETURN_THROWS();
    }

    if (status == 0) {
        RETURN_NULL();
    }

    RETURN_ZVAL(&frame, 0, 1);
}

PHP_METHOD(Http2FrameDecoder, drain)
{
    http2_frame_decoder_object *decoder = Z_HTTP2_FRAME_DECODER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    array_init(return_value);

    while (1) {
        zval frame;
        int status;

        ZVAL_UNDEF(&frame);
        status = http2_frame_decoder_next_internal(decoder, &frame);

        if (status < 0) {
            zval_ptr_dtor(return_value);
            RETURN_THROWS();
        }

        if (status == 0) {
            return;
        }

        add_next_index_zval(return_value, &frame);
    }
}

PHP_METHOD(Http2FrameDecoder, reset)
{
    http2_frame_decoder_object *decoder = Z_HTTP2_FRAME_DECODER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    if (decoder->buffer != NULL) {
        zend_string_release(decoder->buffer);
        decoder->buffer = NULL;
    }

    decoder->offset = 0;
}

PHP_METHOD(Http2FrameDecoder, getBufferedLength)
{
    http2_frame_decoder_object *decoder = Z_HTTP2_FRAME_DECODER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    if (decoder->buffer == NULL || decoder->offset >= ZSTR_LEN(decoder->buffer)) {
        RETURN_LONG(0);
    }

    RETURN_LONG((zend_long) (ZSTR_LEN(decoder->buffer) - decoder->offset));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_http2_decoder_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_decoder_push, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, chunk, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_http2_decoder_next_frame, 0, 0, Varion\\Http2\\Frame, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_decoder_drain, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_decoder_reset, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_decoder_get_buffered_length, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry http2_frame_decoder_methods[] = {
    PHP_ME(Http2FrameDecoder, __construct, arginfo_http2_decoder_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameDecoder, push, arginfo_http2_decoder_push, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameDecoder, nextFrame, arginfo_http2_decoder_next_frame, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameDecoder, drain, arginfo_http2_decoder_drain, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameDecoder, reset, arginfo_http2_decoder_reset, ZEND_ACC_PUBLIC)
    PHP_ME(Http2FrameDecoder, getBufferedLength, arginfo_http2_decoder_get_buffered_length, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static void http2_frame_decoder_free_object(zend_object *object)
{
    http2_frame_decoder_object *decoder = http2_frame_decoder_fetch_object(object);

    if (decoder->buffer != NULL) {
        zend_string_release(decoder->buffer);
        decoder->buffer = NULL;
    }

    zend_object_std_dtor(&decoder->std);
}

static zend_object *http2_frame_decoder_create_object(zend_class_entry *ce)
{
    http2_frame_decoder_object *decoder = zend_object_alloc(sizeof(http2_frame_decoder_object), ce);

    decoder->buffer = NULL;
    decoder->offset = 0;

    zend_object_std_init(&decoder->std, ce);
    object_properties_init(&decoder->std, ce);
    decoder->std.handlers = &http2_frame_decoder_handlers;

    return &decoder->std;
}

void http2_register_frame_decoder_class(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "FrameDecoder", http2_frame_decoder_methods);
    http2_ce_frame_decoder = zend_register_internal_class(&ce);
    http2_ce_frame_decoder->create_object = http2_frame_decoder_create_object;

    memcpy(&http2_frame_decoder_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    http2_frame_decoder_handlers.offset = XtOffsetOf(http2_frame_decoder_object, std);
    http2_frame_decoder_handlers.free_obj = http2_frame_decoder_free_object;
}
