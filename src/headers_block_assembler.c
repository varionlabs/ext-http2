#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_http2.h"

typedef struct _http2_headers_block_assembler_object {
    zend_string *fragment;
    zend_long stream_id;
    bool started;
    bool complete;
    zend_object std;
} http2_headers_block_assembler_object;

static zend_object_handlers http2_headers_block_assembler_handlers;

static inline http2_headers_block_assembler_object *http2_headers_block_assembler_fetch_object(zend_object *obj)
{
    return (http2_headers_block_assembler_object *) ((char *) obj - XtOffsetOf(http2_headers_block_assembler_object, std));
}

#define Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(zv) http2_headers_block_assembler_fetch_object(Z_OBJ_P((zv)))

zend_class_entry *http2_ce_headers_block_assembler;

static zval *http2_assembler_read_frame_property(zval *frame, zend_class_entry *ce, const char *name, size_t name_len)
{
    return zend_read_property(ce, Z_OBJ_P(frame), name, name_len, 1, NULL);
}

static void http2_assembler_append_fragment(http2_headers_block_assembler_object *assembler, zend_string *part)
{
    size_t current_length;
    size_t part_length = ZSTR_LEN(part);

    if (part_length == 0) {
        return;
    }

    if (assembler->fragment == NULL) {
        assembler->fragment = zend_string_init(ZSTR_VAL(part), part_length, 0);
        return;
    }

    current_length = ZSTR_LEN(assembler->fragment);
    assembler->fragment = zend_string_extend(assembler->fragment, current_length + part_length, 0);
    memcpy(ZSTR_VAL(assembler->fragment) + current_length, ZSTR_VAL(part), part_length);
    ZSTR_VAL(assembler->fragment)[current_length + part_length] = '\0';
}

static void http2_headers_block_assembler_reset_internal(http2_headers_block_assembler_object *assembler)
{
    if (assembler->fragment != NULL) {
        zend_string_release(assembler->fragment);
        assembler->fragment = NULL;
    }

    assembler->stream_id = 0;
    assembler->started = false;
    assembler->complete = false;
}

PHP_METHOD(Http2HeadersBlockAssembler, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(Http2HeadersBlockAssembler, push)
{
    zval *frame;
    http2_headers_block_assembler_object *assembler = Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(ZEND_THIS);
    zend_class_entry *frame_ce;
    zval *flags_prop;
    zval *stream_id_prop;
    zend_long flags;
    zend_long stream_id;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(frame, http2_ce_frame)
    ZEND_PARSE_PARAMETERS_END();

    frame_ce = Z_OBJCE_P(frame);
    flags_prop = http2_assembler_read_frame_property(frame, http2_ce_frame, ZEND_STRL("flags"));
    stream_id_prop = http2_assembler_read_frame_property(frame, http2_ce_frame, ZEND_STRL("streamId"));
    flags = zval_get_long(flags_prop);
    stream_id = zval_get_long(stream_id_prop);

    if (!assembler->started) {
        zval *fragment_prop;

        if (!instanceof_function(frame_ce, http2_ce_headers_frame)) {
            zend_throw_exception_ex(http2_ce_stream_state_error_exception, 0, "First frame must be a HeadersFrame");
            RETURN_THROWS();
        }

        fragment_prop = http2_assembler_read_frame_property(frame, http2_ce_headers_frame, ZEND_STRL("headerBlockFragment"));

        assembler->started = true;
        assembler->stream_id = stream_id;
        assembler->complete = false;

        if (Z_TYPE_P(fragment_prop) == IS_STRING) {
            http2_assembler_append_fragment(assembler, Z_STR_P(fragment_prop));
        }

        if ((flags & HTTP2_FLAG_END_HEADERS) != 0) {
            assembler->complete = true;
        }

        return;
    }

    if (assembler->complete) {
        zend_throw_exception_ex(http2_ce_stream_state_error_exception, 0, "Header block is already complete; call reset() before pushing more frames");
        RETURN_THROWS();
    }

    if (!instanceof_function(frame_ce, http2_ce_continuation_frame)) {
        zend_throw_exception_ex(http2_ce_stream_state_error_exception, 0, "Expected a ContinuationFrame while assembling header block");
        RETURN_THROWS();
    }

    if (stream_id != assembler->stream_id) {
        zend_throw_exception_ex(
            http2_ce_stream_state_error_exception,
            0,
            "ContinuationFrame stream ID %ld does not match current stream ID %ld",
            stream_id,
            assembler->stream_id
        );
        RETURN_THROWS();
    }

    {
        zval *fragment_prop = http2_assembler_read_frame_property(frame, http2_ce_continuation_frame, ZEND_STRL("headerBlockFragment"));
        if (Z_TYPE_P(fragment_prop) == IS_STRING) {
            http2_assembler_append_fragment(assembler, Z_STR_P(fragment_prop));
        }
    }

    if ((flags & HTTP2_FLAG_END_HEADERS) != 0) {
        assembler->complete = true;
    }
}

PHP_METHOD(Http2HeadersBlockAssembler, isComplete)
{
    http2_headers_block_assembler_object *assembler = Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(assembler->complete);
}

PHP_METHOD(Http2HeadersBlockAssembler, getHeaderBlockFragment)
{
    http2_headers_block_assembler_object *assembler = Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    if (!assembler->started) {
        zend_throw_exception_ex(http2_ce_stream_state_error_exception, 0, "No header block has been started");
        RETURN_THROWS();
    }

    if (assembler->fragment == NULL) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(assembler->fragment);
}

PHP_METHOD(Http2HeadersBlockAssembler, getStreamId)
{
    http2_headers_block_assembler_object *assembler = Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    if (!assembler->started) {
        zend_throw_exception_ex(http2_ce_stream_state_error_exception, 0, "No header block has been started");
        RETURN_THROWS();
    }

    RETURN_LONG(assembler->stream_id);
}

PHP_METHOD(Http2HeadersBlockAssembler, reset)
{
    http2_headers_block_assembler_object *assembler = Z_HTTP2_HEADERS_BLOCK_ASSEMBLER_OBJ_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_NONE();

    http2_headers_block_assembler_reset_internal(assembler);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_http2_assembler_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_assembler_push, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, frame, Varion\\Http2\\Frame, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_assembler_is_complete, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_assembler_get_header_block_fragment, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_assembler_get_stream_id, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_assembler_reset, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry http2_headers_block_assembler_methods[] = {
    PHP_ME(Http2HeadersBlockAssembler, __construct, arginfo_http2_assembler_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersBlockAssembler, push, arginfo_http2_assembler_push, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersBlockAssembler, isComplete, arginfo_http2_assembler_is_complete, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersBlockAssembler, getHeaderBlockFragment, arginfo_http2_assembler_get_header_block_fragment, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersBlockAssembler, getStreamId, arginfo_http2_assembler_get_stream_id, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersBlockAssembler, reset, arginfo_http2_assembler_reset, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static void http2_headers_block_assembler_free_object(zend_object *object)
{
    http2_headers_block_assembler_object *assembler = http2_headers_block_assembler_fetch_object(object);

    http2_headers_block_assembler_reset_internal(assembler);
    zend_object_std_dtor(&assembler->std);
}

static zend_object *http2_headers_block_assembler_create_object(zend_class_entry *ce)
{
    http2_headers_block_assembler_object *assembler = zend_object_alloc(sizeof(http2_headers_block_assembler_object), ce);

    assembler->fragment = NULL;
    assembler->stream_id = 0;
    assembler->started = false;
    assembler->complete = false;

    zend_object_std_init(&assembler->std, ce);
    object_properties_init(&assembler->std, ce);
    assembler->std.handlers = &http2_headers_block_assembler_handlers;

    return &assembler->std;
}

void http2_register_headers_block_assembler_class(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "HeadersBlockAssembler", http2_headers_block_assembler_methods);
    http2_ce_headers_block_assembler = zend_register_internal_class(&ce);
    http2_ce_headers_block_assembler->create_object = http2_headers_block_assembler_create_object;

    memcpy(&http2_headers_block_assembler_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    http2_headers_block_assembler_handlers.offset = XtOffsetOf(http2_headers_block_assembler_object, std);
    http2_headers_block_assembler_handlers.free_obj = http2_headers_block_assembler_free_object;
}
