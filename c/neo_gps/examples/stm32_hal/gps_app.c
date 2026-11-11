/*
 * STM32 HAL-Beispiel (CubeMX: USARTx 9600 8N1, NVIC-Interrupt an).
 * Bytes kommen per Interrupt in einen Ringpuffer, geparst wird in der Hauptschleife.
 * (neo_gps_feed() ist kurz genug, um es auch direkt in der ISR aufzurufen.)
 *
 * In main():  gps_app_init();  while (1) { gps_app_poll(); ... }
 */
#include "main.h"
#include "neo_gps.h"

extern UART_HandleTypeDef huart2;          /* an das Modul angeschlossene UART */
#define GPS_UART (&huart2)

static neo_gps_t gps;
static uint8_t rx_byte;
static volatile uint8_t ring[256];         /* 256 -> uint8_t-Indizes laufen automatisch um */
static volatile uint8_t head, tail;

static void gps_write(void *user, const uint8_t *data, uint16_t len)
{
    (void)user;
    HAL_UART_Transmit(GPS_UART, (uint8_t *)data, len, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == GPS_UART) {
        ring[head++] = rx_byte;
        HAL_UART_Receive_IT(GPS_UART, &rx_byte, 1);
    }
}

void gps_app_init(void)
{
    neo_gps_init(&gps, gps_write, NULL);
    HAL_UART_Receive_IT(GPS_UART, &rx_byte, 1);

    neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL);
    neo_gps_cfg_dynmodel(&gps, NEO_DYN_AUTOMOTIVE);
    neo_gps_cfg_rate(&gps, 200);           /* 5 Hz - geht auf allen drei Modulen */
}

void gps_app_poll(void)
{
    while (tail != head)
        neo_gps_feed(&gps, ring[tail++]);

    if (neo_gps_updates(&gps) & NEO_UPD_POSITION) {
        const neo_gps_data_t *d = neo_gps_data(&gps);
        /* d->lat / d->lon in 1e-7 Grad, d->alt_msl in mm, d->speed in mm/s ... */
        (void)d;
    }
}
