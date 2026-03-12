#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"
#include "php_http2.h"

zend_class_entry *http2_ce_frame_type_constants;
zend_class_entry *http2_ce_flag_constants;
zend_class_entry *http2_ce_exception;
zend_class_entry *http2_ce_frame_error_exception;
zend_class_entry *http2_ce_payload_length_error_exception;
zend_class_entry *http2_ce_stream_state_error_exception;
zend_class_entry *http2_ce_frame;
zend_class_entry *http2_ce_data_frame;
zend_class_entry *http2_ce_headers_frame;
zend_class_entry *http2_ce_priority_frame;
zend_class_entry *http2_ce_rst_stream_frame;
zend_class_entry *http2_ce_push_promise_frame;
zend_class_entry *http2_ce_settings_frame;
zend_class_entry *http2_ce_ping_frame;
zend_class_entry *http2_ce_goaway_frame;
zend_class_entry *http2_ce_window_update_frame;
zend_class_entry *http2_ce_continuation_frame;
zend_class_entry *http2_ce_unknown_frame;

static uint16_t http2_read_u16be(const unsigned char *p)
{
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static uint32_t http2_read_u32be(const unsigned char *p)
{
    return ((uint32_t) p[0] << 24)
        | ((uint32_t) p[1] << 16)
        | ((uint32_t) p[2] << 8)
        | (uint32_t) p[3];
}

static zend_result http2_uint32_to_zend_long(uint32_t value, zend_long *out, const char *field_name)
{
#if SIZEOF_ZEND_LONG < 8
    if (value > (uint32_t) ZEND_LONG_MAX) {
        zend_throw_exception_ex(
            http2_ce_frame_error_exception,
            0,
            "%s value %u is not representable as int on this platform",
            field_name,
            value
        );
        return FAILURE;
    }
#endif

    *out = (zend_long) value;
    return SUCCESS;
}

static void http2_update_nullable_long(zend_class_entry *ce, zval *obj, const char *name, size_t name_len, bool present, zend_long value)
{
    zval zv;

    if (present) {
        ZVAL_LONG(&zv, value);
    } else {
        ZVAL_NULL(&zv);
    }

    zend_update_property(ce, Z_OBJ_P(obj), name, name_len, &zv);
}

static void http2_set_common_frame_properties(
    zval *obj,
    uint8_t type,
    uint8_t flags,
    uint32_t stream_id,
    const unsigned char *payload,
    size_t payload_length
)
{
    zend_update_property_long(http2_ce_frame, Z_OBJ_P(obj), ZEND_STRL("type"), (zend_long) type);
    zend_update_property_long(http2_ce_frame, Z_OBJ_P(obj), ZEND_STRL("flags"), (zend_long) flags);
    zend_update_property_long(http2_ce_frame, Z_OBJ_P(obj), ZEND_STRL("streamId"), (zend_long) stream_id);
    zend_update_property_long(http2_ce_frame, Z_OBJ_P(obj), ZEND_STRL("payloadLength"), (zend_long) payload_length);
    zend_update_property_stringl(http2_ce_frame, Z_OBJ_P(obj), ZEND_STRL("payload"), (const char *) payload, payload_length);
}

static zend_result http2_parse_headers_frame(
    zval *obj,
    uint8_t flags,
    const unsigned char *payload,
    size_t payload_length
)
{
    size_t cursor = 0;
    size_t remaining = payload_length;
    bool has_padding = (flags & HTTP2_FLAG_PADDED) != 0;
    bool has_priority = (flags & HTTP2_FLAG_PRIORITY) != 0;
    uint8_t pad_length = 0;
    bool exclusive = false;
    uint32_t stream_dependency = 0;
    uint8_t weight = 0;
    size_t header_block_length;

    if (has_padding) {
        if (remaining < 1) {
            zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "HEADERS frame is PADDED but payload is too short");
            return FAILURE;
        }

        pad_length = payload[cursor];
        cursor++;
        remaining--;

        zend_update_property_long(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("padLength"), (zend_long) pad_length);
    } else {
        http2_update_nullable_long(http2_ce_headers_frame, obj, ZEND_STRL("padLength"), false, 0);
    }

    zend_update_property_bool(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("hasPriority"), has_priority);

    if (has_priority) {
        uint32_t dependency_raw;

        if (remaining < 5) {
            zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "HEADERS frame has PRIORITY flag but payload is too short");
            return FAILURE;
        }

        dependency_raw = http2_read_u32be(payload + cursor);
        exclusive = (dependency_raw & 0x80000000U) != 0;
        stream_dependency = dependency_raw & 0x7FFFFFFFU;
        weight = payload[cursor + 4];
        cursor += 5;
        remaining -= 5;

        zend_update_property_bool(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("exclusive"), exclusive);
        zend_update_property_long(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("streamDependency"), (zend_long) stream_dependency);
        zend_update_property_long(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("weight"), (zend_long) weight);
    } else {
        zend_update_property_bool(http2_ce_headers_frame, Z_OBJ_P(obj), ZEND_STRL("exclusive"), false);
        http2_update_nullable_long(http2_ce_headers_frame, obj, ZEND_STRL("streamDependency"), false, 0);
        http2_update_nullable_long(http2_ce_headers_frame, obj, ZEND_STRL("weight"), false, 0);
    }

    if (has_padding && (size_t) pad_length > remaining) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "HEADERS frame pad length exceeds remaining payload");
        return FAILURE;
    }

    header_block_length = remaining - (size_t) pad_length;
    zend_update_property_stringl(
        http2_ce_headers_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("headerBlockFragment"),
        (const char *) (payload + cursor),
        header_block_length
    );

    return SUCCESS;
}

static zend_result http2_parse_continuation_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    zend_update_property_stringl(
        http2_ce_continuation_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("headerBlockFragment"),
        (const char *) payload,
        payload_length
    );

    return SUCCESS;
}

static zend_result http2_parse_priority_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    uint32_t dependency_raw;
    bool exclusive;
    uint32_t stream_dependency;
    uint8_t weight;

    if (payload_length != 5) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "PRIORITY payload length must be exactly 5 bytes");
        return FAILURE;
    }

    dependency_raw = http2_read_u32be(payload);
    exclusive = (dependency_raw & 0x80000000U) != 0;
    stream_dependency = dependency_raw & 0x7FFFFFFFU;
    weight = payload[4];

    zend_update_property_bool(http2_ce_priority_frame, Z_OBJ_P(obj), ZEND_STRL("exclusive"), exclusive);
    zend_update_property_long(http2_ce_priority_frame, Z_OBJ_P(obj), ZEND_STRL("streamDependency"), (zend_long) stream_dependency);
    zend_update_property_long(http2_ce_priority_frame, Z_OBJ_P(obj), ZEND_STRL("weight"), (zend_long) weight);

    return SUCCESS;
}

static zend_result http2_parse_rst_stream_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    uint32_t error_code_raw;
    zend_long error_code;

    if (payload_length != 4) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "RST_STREAM payload length must be exactly 4 bytes");
        return FAILURE;
    }

    error_code_raw = http2_read_u32be(payload);
    if (http2_uint32_to_zend_long(error_code_raw, &error_code, "RST_STREAM error code") != SUCCESS) {
        return FAILURE;
    }

    zend_update_property_long(http2_ce_rst_stream_frame, Z_OBJ_P(obj), ZEND_STRL("errorCode"), error_code);

    return SUCCESS;
}

static zend_result http2_parse_push_promise_frame(
    zval *obj,
    uint8_t flags,
    const unsigned char *payload,
    size_t payload_length
)
{
    bool has_padding = (flags & HTTP2_FLAG_PADDED) != 0;
    size_t cursor = 0;
    size_t remaining = payload_length;
    uint8_t pad_length = 0;
    uint32_t promised_stream_id_raw;
    uint32_t promised_stream_id;
    size_t header_block_length;

    if (has_padding) {
        if (remaining < 1) {
            zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "PUSH_PROMISE frame is PADDED but payload is too short");
            return FAILURE;
        }

        pad_length = payload[cursor];
        cursor++;
        remaining--;
        zend_update_property_long(http2_ce_push_promise_frame, Z_OBJ_P(obj), ZEND_STRL("padLength"), (zend_long) pad_length);
    } else {
        http2_update_nullable_long(http2_ce_push_promise_frame, obj, ZEND_STRL("padLength"), false, 0);
    }

    if (remaining < 4) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "PUSH_PROMISE payload is too short for promised stream ID");
        return FAILURE;
    }

    promised_stream_id_raw = http2_read_u32be(payload + cursor);
    promised_stream_id = promised_stream_id_raw & 0x7FFFFFFFU;
    cursor += 4;
    remaining -= 4;

    if (has_padding && (size_t) pad_length > remaining) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "PUSH_PROMISE pad length exceeds remaining payload");
        return FAILURE;
    }

    header_block_length = remaining - (size_t) pad_length;

    zend_update_property_long(
        http2_ce_push_promise_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("promisedStreamId"),
        (zend_long) promised_stream_id
    );
    zend_update_property_stringl(
        http2_ce_push_promise_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("headerBlockFragment"),
        (const char *) (payload + cursor),
        header_block_length
    );

    return SUCCESS;
}

static zend_result http2_parse_settings_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    size_t i;
    zval settings;

    if (payload_length % 6 != 0) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "SETTINGS payload length must be a multiple of 6 bytes");
        return FAILURE;
    }

    array_init(&settings);

    for (i = 0; i < payload_length; i += 6) {
        uint16_t id = http2_read_u16be(payload + i);
        uint32_t value = http2_read_u32be(payload + i + 2);
        zend_long value_long;

        if (http2_uint32_to_zend_long(value, &value_long, "SETTINGS value") != SUCCESS) {
            zval_ptr_dtor(&settings);
            return FAILURE;
        }

        add_index_long(&settings, (zend_long) id, value_long);
    }

    zend_update_property(http2_ce_settings_frame, Z_OBJ_P(obj), ZEND_STRL("settings"), &settings);
    zval_ptr_dtor(&settings);

    return SUCCESS;
}

static zend_result http2_parse_ping_frame(size_t payload_length)
{
    if (payload_length != 8) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "PING payload length must be exactly 8 bytes");
        return FAILURE;
    }

    return SUCCESS;
}

static zend_result http2_parse_window_update_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    uint32_t raw;
    uint32_t increment;

    if (payload_length != 4) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "WINDOW_UPDATE payload length must be exactly 4 bytes");
        return FAILURE;
    }

    raw = http2_read_u32be(payload);
    increment = raw & 0x7FFFFFFFU;

    if (increment == 0) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "WINDOW_UPDATE increment must be non-zero");
        return FAILURE;
    }

    zend_update_property_long(http2_ce_window_update_frame, Z_OBJ_P(obj), ZEND_STRL("windowSizeIncrement"), (zend_long) increment);

    return SUCCESS;
}

static zend_result http2_parse_goaway_frame(zval *obj, const unsigned char *payload, size_t payload_length)
{
    uint32_t last_stream_id_raw;
    uint32_t error_code_raw;
    zend_long error_code;

    if (payload_length < 8) {
        zend_throw_exception_ex(http2_ce_payload_length_error_exception, 0, "GOAWAY payload length must be at least 8 bytes");
        return FAILURE;
    }

    last_stream_id_raw = http2_read_u32be(payload);
    error_code_raw = http2_read_u32be(payload + 4);

    if (http2_uint32_to_zend_long(error_code_raw, &error_code, "GOAWAY error code") != SUCCESS) {
        return FAILURE;
    }

    zend_update_property_long(
        http2_ce_goaway_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("lastStreamId"),
        (zend_long) (last_stream_id_raw & 0x7FFFFFFFU)
    );
    zend_update_property_long(http2_ce_goaway_frame, Z_OBJ_P(obj), ZEND_STRL("errorCode"), error_code);
    zend_update_property_stringl(
        http2_ce_goaway_frame,
        Z_OBJ_P(obj),
        ZEND_STRL("debugData"),
        (const char *) (payload + 8),
        payload_length - 8
    );

    return SUCCESS;
}

zend_result http2_create_frame_object(
    uint8_t type,
    uint8_t flags,
    uint32_t stream_id,
    const unsigned char *payload,
    size_t payload_length,
    zval *return_value
)
{
    zend_class_entry *target_ce = http2_ce_unknown_frame;

    switch (type) {
        case HTTP2_FRAME_TYPE_DATA:
            target_ce = http2_ce_data_frame;
            break;
        case HTTP2_FRAME_TYPE_HEADERS:
            target_ce = http2_ce_headers_frame;
            break;
        case HTTP2_FRAME_TYPE_PRIORITY:
            target_ce = http2_ce_priority_frame;
            break;
        case HTTP2_FRAME_TYPE_RST_STREAM:
            target_ce = http2_ce_rst_stream_frame;
            break;
        case HTTP2_FRAME_TYPE_PUSH_PROMISE:
            target_ce = http2_ce_push_promise_frame;
            break;
        case HTTP2_FRAME_TYPE_SETTINGS:
            target_ce = http2_ce_settings_frame;
            break;
        case HTTP2_FRAME_TYPE_PING:
            target_ce = http2_ce_ping_frame;
            break;
        case HTTP2_FRAME_TYPE_GOAWAY:
            target_ce = http2_ce_goaway_frame;
            break;
        case HTTP2_FRAME_TYPE_WINDOW_UPDATE:
            target_ce = http2_ce_window_update_frame;
            break;
        case HTTP2_FRAME_TYPE_CONTINUATION:
            target_ce = http2_ce_continuation_frame;
            break;
        default:
            target_ce = http2_ce_unknown_frame;
            break;
    }

    object_init_ex(return_value, target_ce);
    http2_set_common_frame_properties(return_value, type, flags, stream_id, payload, payload_length);

    switch (type) {
        case HTTP2_FRAME_TYPE_HEADERS:
            if (http2_parse_headers_frame(return_value, flags, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_PRIORITY:
            if (http2_parse_priority_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_RST_STREAM:
            if (http2_parse_rst_stream_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_PUSH_PROMISE:
            if (http2_parse_push_promise_frame(return_value, flags, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_CONTINUATION:
            if (http2_parse_continuation_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_SETTINGS:
            if (http2_parse_settings_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_PING:
            if (http2_parse_ping_frame(payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_WINDOW_UPDATE:
            if (http2_parse_window_update_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        case HTTP2_FRAME_TYPE_GOAWAY:
            if (http2_parse_goaway_frame(return_value, payload, payload_length) != SUCCESS) {
                zval_ptr_dtor(return_value);
                ZVAL_NULL(return_value);
                return FAILURE;
            }
            break;

        default:
            break;
    }

    return SUCCESS;
}

static zval *http2_read_property(zval *obj, zend_class_entry *ce, const char *name, size_t name_len)
{
    return zend_read_property(ce, Z_OBJ_P(obj), name, name_len, 1, NULL);
}

PHP_METHOD(Http2Frame, getType)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("type"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2Frame, getFlags)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("flags"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2Frame, getStreamId)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("streamId"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2Frame, getPayloadLength)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("payloadLength"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2Frame, getPayload)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("payload"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

PHP_METHOD(Http2HeadersFrame, getPadLength)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("padLength"));
    if (Z_TYPE_P(property) == IS_NULL) {
        RETURN_NULL();
    }
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2HeadersFrame, hasPriority)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("hasPriority"));
    RETURN_BOOL(zval_is_true(property));
}

PHP_METHOD(Http2HeadersFrame, isExclusive)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("exclusive"));
    RETURN_BOOL(zval_is_true(property));
}

PHP_METHOD(Http2HeadersFrame, getStreamDependency)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("streamDependency"));
    if (Z_TYPE_P(property) == IS_NULL) {
        RETURN_NULL();
    }
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2HeadersFrame, getWeight)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("weight"));
    if (Z_TYPE_P(property) == IS_NULL) {
        RETURN_NULL();
    }
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2HeadersFrame, getHeaderBlockFragment)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_headers_frame, ZEND_STRL("headerBlockFragment"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

PHP_METHOD(Http2PriorityFrame, isExclusive)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_priority_frame, ZEND_STRL("exclusive"));
    RETURN_BOOL(zval_is_true(property));
}

PHP_METHOD(Http2PriorityFrame, getStreamDependency)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_priority_frame, ZEND_STRL("streamDependency"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2PriorityFrame, getWeight)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_priority_frame, ZEND_STRL("weight"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2RstStreamFrame, getErrorCode)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_rst_stream_frame, ZEND_STRL("errorCode"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2PushPromiseFrame, getPadLength)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_push_promise_frame, ZEND_STRL("padLength"));
    if (Z_TYPE_P(property) == IS_NULL) {
        RETURN_NULL();
    }
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2PushPromiseFrame, getPromisedStreamId)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_push_promise_frame, ZEND_STRL("promisedStreamId"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2PushPromiseFrame, getHeaderBlockFragment)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_push_promise_frame, ZEND_STRL("headerBlockFragment"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

PHP_METHOD(Http2ContinuationFrame, getHeaderBlockFragment)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_continuation_frame, ZEND_STRL("headerBlockFragment"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

PHP_METHOD(Http2SettingsFrame, getSettings)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_settings_frame, ZEND_STRL("settings"));

    if (Z_TYPE_P(property) != IS_ARRAY) {
        array_init(return_value);
        return;
    }

    RETURN_ARR(zend_array_dup(Z_ARR_P(property)));
}

PHP_METHOD(Http2PingFrame, getOpaqueData)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_frame, ZEND_STRL("payload"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

PHP_METHOD(Http2WindowUpdateFrame, getWindowSizeIncrement)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_window_update_frame, ZEND_STRL("windowSizeIncrement"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2GoawayFrame, getLastStreamId)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_goaway_frame, ZEND_STRL("lastStreamId"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2GoawayFrame, getErrorCode)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_goaway_frame, ZEND_STRL("errorCode"));
    RETURN_LONG(zval_get_long(property));
}

PHP_METHOD(Http2GoawayFrame, getDebugData)
{
    zval *property;

    ZEND_PARSE_PARAMETERS_NONE();
    property = http2_read_property(ZEND_THIS, http2_ce_goaway_frame, ZEND_STRL("debugData"));

    if (Z_TYPE_P(property) != IS_STRING) {
        RETURN_EMPTY_STRING();
    }

    RETURN_STR_COPY(Z_STR_P(property));
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_frame_get_type, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_http2_frame_get_flags arginfo_http2_frame_get_type
#define arginfo_http2_frame_get_stream_id arginfo_http2_frame_get_type
#define arginfo_http2_frame_get_payload_length arginfo_http2_frame_get_type

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_frame_get_payload, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_headers_get_pad_length, 0, 0, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_headers_has_priority, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_http2_headers_is_exclusive arginfo_http2_headers_has_priority

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_headers_get_stream_dependency, 0, 0, IS_LONG, 1)
ZEND_END_ARG_INFO()

#define arginfo_http2_headers_get_weight arginfo_http2_headers_get_stream_dependency

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_headers_get_header_block_fragment, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_priority_is_exclusive, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

#define arginfo_http2_priority_get_stream_dependency arginfo_http2_frame_get_type
#define arginfo_http2_priority_get_weight arginfo_http2_frame_get_type
#define arginfo_http2_rst_stream_get_error_code arginfo_http2_frame_get_type
#define arginfo_http2_push_promise_get_pad_length arginfo_http2_headers_get_pad_length
#define arginfo_http2_push_promise_get_promised_stream_id arginfo_http2_frame_get_type
#define arginfo_http2_push_promise_get_header_block_fragment arginfo_http2_headers_get_header_block_fragment

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_http2_settings_get_settings, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

#define arginfo_http2_ping_get_opaque_data arginfo_http2_frame_get_payload
#define arginfo_http2_window_update_get_window_size_increment arginfo_http2_frame_get_type
#define arginfo_http2_goaway_get_last_stream_id arginfo_http2_frame_get_type
#define arginfo_http2_goaway_get_error_code arginfo_http2_frame_get_type
#define arginfo_http2_goaway_get_debug_data arginfo_http2_frame_get_payload

static const zend_function_entry http2_frame_methods[] = {
    PHP_ME(Http2Frame, getType, arginfo_http2_frame_get_type, ZEND_ACC_PUBLIC)
    PHP_ME(Http2Frame, getFlags, arginfo_http2_frame_get_flags, ZEND_ACC_PUBLIC)
    PHP_ME(Http2Frame, getStreamId, arginfo_http2_frame_get_stream_id, ZEND_ACC_PUBLIC)
    PHP_ME(Http2Frame, getPayloadLength, arginfo_http2_frame_get_payload_length, ZEND_ACC_PUBLIC)
    PHP_ME(Http2Frame, getPayload, arginfo_http2_frame_get_payload, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_headers_frame_methods[] = {
    PHP_ME(Http2HeadersFrame, getPadLength, arginfo_http2_headers_get_pad_length, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersFrame, hasPriority, arginfo_http2_headers_has_priority, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersFrame, isExclusive, arginfo_http2_headers_is_exclusive, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersFrame, getStreamDependency, arginfo_http2_headers_get_stream_dependency, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersFrame, getWeight, arginfo_http2_headers_get_weight, ZEND_ACC_PUBLIC)
    PHP_ME(Http2HeadersFrame, getHeaderBlockFragment, arginfo_http2_headers_get_header_block_fragment, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_priority_frame_methods[] = {
    PHP_ME(Http2PriorityFrame, isExclusive, arginfo_http2_priority_is_exclusive, ZEND_ACC_PUBLIC)
    PHP_ME(Http2PriorityFrame, getStreamDependency, arginfo_http2_priority_get_stream_dependency, ZEND_ACC_PUBLIC)
    PHP_ME(Http2PriorityFrame, getWeight, arginfo_http2_priority_get_weight, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_rst_stream_frame_methods[] = {
    PHP_ME(Http2RstStreamFrame, getErrorCode, arginfo_http2_rst_stream_get_error_code, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_push_promise_frame_methods[] = {
    PHP_ME(Http2PushPromiseFrame, getPadLength, arginfo_http2_push_promise_get_pad_length, ZEND_ACC_PUBLIC)
    PHP_ME(Http2PushPromiseFrame, getPromisedStreamId, arginfo_http2_push_promise_get_promised_stream_id, ZEND_ACC_PUBLIC)
    PHP_ME(Http2PushPromiseFrame, getHeaderBlockFragment, arginfo_http2_push_promise_get_header_block_fragment, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_continuation_frame_methods[] = {
    PHP_ME(Http2ContinuationFrame, getHeaderBlockFragment, arginfo_http2_headers_get_header_block_fragment, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_settings_frame_methods[] = {
    PHP_ME(Http2SettingsFrame, getSettings, arginfo_http2_settings_get_settings, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_ping_frame_methods[] = {
    PHP_ME(Http2PingFrame, getOpaqueData, arginfo_http2_ping_get_opaque_data, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_window_update_frame_methods[] = {
    PHP_ME(Http2WindowUpdateFrame, getWindowSizeIncrement, arginfo_http2_window_update_get_window_size_increment, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry http2_goaway_frame_methods[] = {
    PHP_ME(Http2GoawayFrame, getLastStreamId, arginfo_http2_goaway_get_last_stream_id, ZEND_ACC_PUBLIC)
    PHP_ME(Http2GoawayFrame, getErrorCode, arginfo_http2_goaway_get_error_code, ZEND_ACC_PUBLIC)
    PHP_ME(Http2GoawayFrame, getDebugData, arginfo_http2_goaway_get_debug_data, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void http2_register_exception_classes(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "Exception", NULL);
    http2_ce_exception = zend_register_internal_class_ex(&ce, zend_ce_exception);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "FrameError", NULL);
    http2_ce_frame_error_exception = zend_register_internal_class_ex(&ce, http2_ce_exception);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "PayloadLengthError", NULL);
    http2_ce_payload_length_error_exception = zend_register_internal_class_ex(&ce, http2_ce_frame_error_exception);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "StreamStateError", NULL);
    http2_ce_stream_state_error_exception = zend_register_internal_class_ex(&ce, http2_ce_frame_error_exception);
}

void http2_register_http2_constants(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "FrameType", NULL);
    http2_ce_frame_type_constants = zend_register_internal_class(&ce);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("DATA"), HTTP2_FRAME_TYPE_DATA);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("HEADERS"), HTTP2_FRAME_TYPE_HEADERS);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("PRIORITY"), HTTP2_FRAME_TYPE_PRIORITY);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("RST_STREAM"), HTTP2_FRAME_TYPE_RST_STREAM);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("SETTINGS"), HTTP2_FRAME_TYPE_SETTINGS);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("PUSH_PROMISE"), HTTP2_FRAME_TYPE_PUSH_PROMISE);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("PING"), HTTP2_FRAME_TYPE_PING);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("GOAWAY"), HTTP2_FRAME_TYPE_GOAWAY);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("WINDOW_UPDATE"), HTTP2_FRAME_TYPE_WINDOW_UPDATE);
    zend_declare_class_constant_long(http2_ce_frame_type_constants, ZEND_STRL("CONTINUATION"), HTTP2_FRAME_TYPE_CONTINUATION);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "Flag", NULL);
    http2_ce_flag_constants = zend_register_internal_class(&ce);
    zend_declare_class_constant_long(http2_ce_flag_constants, ZEND_STRL("END_STREAM"), HTTP2_FLAG_END_STREAM);
    zend_declare_class_constant_long(http2_ce_flag_constants, ZEND_STRL("ACK"), HTTP2_FLAG_ACK);
    zend_declare_class_constant_long(http2_ce_flag_constants, ZEND_STRL("END_HEADERS"), HTTP2_FLAG_END_HEADERS);
    zend_declare_class_constant_long(http2_ce_flag_constants, ZEND_STRL("PADDED"), HTTP2_FLAG_PADDED);
    zend_declare_class_constant_long(http2_ce_flag_constants, ZEND_STRL("PRIORITY"), HTTP2_FLAG_PRIORITY);
}

void http2_register_frame_classes(void)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "Frame", http2_frame_methods);
    http2_ce_frame = zend_register_internal_class(&ce);
    zend_declare_property_null(http2_ce_frame, ZEND_STRL("type"), ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_frame, ZEND_STRL("flags"), ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_frame, ZEND_STRL("streamId"), ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_frame, ZEND_STRL("payloadLength"), ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_frame, ZEND_STRL("payload"), ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "DataFrame", NULL);
    http2_ce_data_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "HeadersFrame", http2_headers_frame_methods);
    http2_ce_headers_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_null(http2_ce_headers_frame, ZEND_STRL("padLength"), ZEND_ACC_PROTECTED);
    zend_declare_property_bool(http2_ce_headers_frame, ZEND_STRL("hasPriority"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_bool(http2_ce_headers_frame, ZEND_STRL("exclusive"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_headers_frame, ZEND_STRL("streamDependency"), ZEND_ACC_PROTECTED);
    zend_declare_property_null(http2_ce_headers_frame, ZEND_STRL("weight"), ZEND_ACC_PROTECTED);
    zend_declare_property_string(http2_ce_headers_frame, ZEND_STRL("headerBlockFragment"), "", ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "PriorityFrame", http2_priority_frame_methods);
    http2_ce_priority_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_bool(http2_ce_priority_frame, ZEND_STRL("exclusive"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_long(http2_ce_priority_frame, ZEND_STRL("streamDependency"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_long(http2_ce_priority_frame, ZEND_STRL("weight"), 0, ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "RstStreamFrame", http2_rst_stream_frame_methods);
    http2_ce_rst_stream_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_long(http2_ce_rst_stream_frame, ZEND_STRL("errorCode"), 0, ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "PushPromiseFrame", http2_push_promise_frame_methods);
    http2_ce_push_promise_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_null(http2_ce_push_promise_frame, ZEND_STRL("padLength"), ZEND_ACC_PROTECTED);
    zend_declare_property_long(http2_ce_push_promise_frame, ZEND_STRL("promisedStreamId"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_string(http2_ce_push_promise_frame, ZEND_STRL("headerBlockFragment"), "", ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "SettingsFrame", http2_settings_frame_methods);
    http2_ce_settings_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_null(http2_ce_settings_frame, ZEND_STRL("settings"), ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "PingFrame", http2_ping_frame_methods);
    http2_ce_ping_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "GoawayFrame", http2_goaway_frame_methods);
    http2_ce_goaway_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_long(http2_ce_goaway_frame, ZEND_STRL("lastStreamId"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_long(http2_ce_goaway_frame, ZEND_STRL("errorCode"), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_string(http2_ce_goaway_frame, ZEND_STRL("debugData"), "", ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "WindowUpdateFrame", http2_window_update_frame_methods);
    http2_ce_window_update_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_long(http2_ce_window_update_frame, ZEND_STRL("windowSizeIncrement"), 0, ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "ContinuationFrame", http2_continuation_frame_methods);
    http2_ce_continuation_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
    zend_declare_property_string(http2_ce_continuation_frame, ZEND_STRL("headerBlockFragment"), "", ZEND_ACC_PROTECTED);

    INIT_NS_CLASS_ENTRY(ce, "Varion\\Http2", "UnknownFrame", NULL);
    http2_ce_unknown_frame = zend_register_internal_class_ex(&ce, http2_ce_frame);
}
