#include "enocean_stream.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdlib.h>

typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
} uart_context_t;

static int hw_uart_init(enocean_stream_t *stream, uint32_t baud) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(ctx->uart_num, &uart_config);
    uart_set_pin(ctx->uart_num, ctx->tx_pin, ctx->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(ctx->uart_num, 2048, 2048, 0, NULL, 0);
    return 0;
}

static int hw_uart_available(enocean_stream_t *stream) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    size_t length = 0;
    uart_get_buffered_data_len(ctx->uart_num, &length);
    return (int)length;
}

static int hw_uart_read(enocean_stream_t *stream) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    uint8_t byte;
    int len = uart_read_bytes(ctx->uart_num, &byte, 1, 0); 
    return (len == 1) ? byte : -1;
}

static size_t hw_uart_read_buf(enocean_stream_t *stream, uint8_t *buf, size_t size) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    int len = uart_read_bytes(ctx->uart_num, buf, size, 0);
    return (len > 0) ? (size_t)len : 0;
}

static size_t hw_uart_write(enocean_stream_t *stream, uint8_t byte) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    int len = uart_write_bytes(ctx->uart_num, (const char*)&byte, 1);
    return (len > 0) ? 1 : 0;
}

static size_t hw_uart_write_buf(enocean_stream_t *stream, const uint8_t *buf, size_t size) {
    uart_context_t *ctx = (uart_context_t *)stream->ctx;
    int len = uart_write_bytes(ctx->uart_num, (const char*)buf, size);
    return (len > 0) ? (size_t)len : 0;
}

enocean_stream_t* get_uart_stream(uart_port_t port, int tx_pin, int rx_pin) {
    static uart_context_t u_ctx;
    static enocean_stream_t u_stream;
    u_ctx.uart_num = port; u_ctx.tx_pin = tx_pin; u_ctx.rx_pin = rx_pin;
    u_stream.ctx = &u_ctx;
    u_stream.init = hw_uart_init; u_stream.available = hw_uart_available; u_stream.read = hw_uart_read;
    u_stream.read_buf = hw_uart_read_buf; u_stream.write = hw_uart_write; u_stream.write_buf = hw_uart_write_buf;
    return &u_stream;
}
