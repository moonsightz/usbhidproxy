#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/mutex.h>
#include <hardware/uart.h>

#include "debug_func.h"

#define ENABLE_DEBUG_PRINTF  0


#define UART_ID  uart1
#define BAUD_RATE  115200

#define UART_TX_PIN  8
#define UART_RX_PIN  9


const size_t cBufSize = 128;


static uart_inst_t *uart_inst;

auto_init_mutex(s_uart_mutex);


int debugInit(void)
{
#if ENABLE_DEBUG_PRINTF
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    uart_init(UART_ID, BAUD_RATE);

    uart_inst = uart_get_instance(1);
    stdio_uart_init_full(uart_inst, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    if ((int)cBufSize < 0) {
        return -1;
    }

#endif
    return 0;
}


int debugPrintf(const char *restrict format, ...)
{
#if ENABLE_DEBUG_PRINTF
    int r;

    char temp[cBufSize + 3];
    va_list ap;
    va_start(ap, format);
    r = vsnprintf(temp, cBufSize, format, ap);
    va_end(ap);
    if (r < 0) {
        return r;
    } else if (r > (int)cBufSize) {
        return -1; // TODO: Is there any better value?
    } else {
        size_t ur = r;
        temp[ur] = '\r';
        temp[ur + 1] = '\n';
        temp[ur + 2] = '\0';

        mutex_enter_blocking(&s_uart_mutex);
        uart_puts(UART_ID, temp);
        mutex_exit(&s_uart_mutex);
    }

    return r;
#else
    return 0;
#endif
}
