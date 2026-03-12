#ifndef PHP_HTTP2_H
#define PHP_HTTP2_H

#include "php.h"

extern zend_module_entry http2_module_entry;
#define phpext_http2_ptr &http2_module_entry

#define PHP_HTTP2_VERSION "0.1.0"

#define HTTP2_FRAME_TYPE_DATA 0x0
#define HTTP2_FRAME_TYPE_HEADERS 0x1
#define HTTP2_FRAME_TYPE_PRIORITY 0x2
#define HTTP2_FRAME_TYPE_RST_STREAM 0x3
#define HTTP2_FRAME_TYPE_SETTINGS 0x4
#define HTTP2_FRAME_TYPE_PUSH_PROMISE 0x5
#define HTTP2_FRAME_TYPE_PING 0x6
#define HTTP2_FRAME_TYPE_GOAWAY 0x7
#define HTTP2_FRAME_TYPE_WINDOW_UPDATE 0x8
#define HTTP2_FRAME_TYPE_CONTINUATION 0x9

#define HTTP2_FLAG_END_STREAM 0x1
#define HTTP2_FLAG_ACK 0x1
#define HTTP2_FLAG_END_HEADERS 0x4
#define HTTP2_FLAG_PADDED 0x8
#define HTTP2_FLAG_PRIORITY 0x20

#define HTTP2_MAX_ALLOWED_FRAME_SIZE (1024 * 1024)

extern zend_class_entry *http2_ce_frame_type_constants;
extern zend_class_entry *http2_ce_flag_constants;
extern zend_class_entry *http2_ce_exception;
extern zend_class_entry *http2_ce_frame_error_exception;
extern zend_class_entry *http2_ce_payload_length_error_exception;
extern zend_class_entry *http2_ce_stream_state_error_exception;
extern zend_class_entry *http2_ce_frame;
extern zend_class_entry *http2_ce_data_frame;
extern zend_class_entry *http2_ce_headers_frame;
extern zend_class_entry *http2_ce_priority_frame;
extern zend_class_entry *http2_ce_rst_stream_frame;
extern zend_class_entry *http2_ce_push_promise_frame;
extern zend_class_entry *http2_ce_settings_frame;
extern zend_class_entry *http2_ce_ping_frame;
extern zend_class_entry *http2_ce_goaway_frame;
extern zend_class_entry *http2_ce_window_update_frame;
extern zend_class_entry *http2_ce_continuation_frame;
extern zend_class_entry *http2_ce_unknown_frame;
extern zend_class_entry *http2_ce_frame_decoder;
extern zend_class_entry *http2_ce_frame_encoder;
extern zend_class_entry *http2_ce_headers_block_assembler;
extern zend_class_entry *http2_ce_event;
extern zend_class_entry *http2_ce_frame_decoded_event;
extern zend_class_entry *http2_ce_headers_block_completed_event;
extern zend_class_entry *http2_ce_decoder_error_event;
extern zend_class_entry *http2_ce_event_factory;

void http2_register_exception_classes(void);
void http2_register_http2_constants(void);
void http2_register_frame_classes(void);
void http2_register_frame_decoder_class(void);
void http2_register_frame_encoder_class(void);
void http2_register_headers_block_assembler_class(void);
void http2_register_event_classes(void);

zend_result http2_create_frame_object(
    uint8_t type,
    uint8_t flags,
    uint32_t stream_id,
    const unsigned char *payload,
    size_t payload_length,
    zval *return_value
);

#endif
