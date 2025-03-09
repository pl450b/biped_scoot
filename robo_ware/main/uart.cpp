#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "biped_types.h"

#define RX_BUF_SIZE           1024

extern QueueHandle_t txQueue;
extern QueueHandle_t rxQueue;

void init_uart(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
}

// Take data from txQueue and send it over UART
void uart_tx_task(void) {
    // test
}

// Task data recieved from UART and place in rxQueue
void uart_rx_task(void) {
    // test
}
