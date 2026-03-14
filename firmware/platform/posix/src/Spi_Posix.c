/**
 * @file    Spi_Posix.c
 * @brief   POSIX SPI stub — implements Spi_Hw_* externs from Spi.h
 * @date    2026-02-23
 *
 * @details Simulates AS5048A angle sensors for SIL.  Channel-aware defaults:
 *          - Channel 0 (pedal):    dead-zone oscillation 200-800 (torque = 0)
 *          - Channel 1+ (steering): center oscillation 8191 ±40 (0° angle)
 *
 *          When SPI_PEDAL_UDP_PORT env var is set (CVC container only),
 *          accepts UDP packets to override the pedal angle for fault
 *          injection scenarios.  Packet: 2 bytes uint16 LE —
 *          angle 0-16383, or 0xFFFF to clear and revert to dead zone.
 *
 * @safety_req SWR-BSW-006: SPI Driver
 * @traces_to  SYS-047, TSR-001, TSR-010
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"

/* ---- POSIX headers for UDP pedal override ---- */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- UDP pedal override constants ---- */
#define SPI_OVERRIDE_CLEAR  0xFFFFu  /**< Packet value to clear override    */
#define SPI_OVERRIDE_STEP   11u      /**< Oscillation step — must be >= stuckThreshold (10) */
#define SPI_OVERRIDE_RANGE  40u      /**< Max offset from target angle       */

/* ---- Default dead-zone oscillation state (Channel 0 / pedal sensors) ---- */

/**
 * @brief  Simulated pedal sensor angle — oscillates to avoid stuck detection
 *
 * Range 200-800 of 14-bit (0-16383) keeps pedal position in the torque
 * dead zone (position < 67 -> torque = 0), so simulated vehicle stays
 * stationary.  Step 11 per Spi_Hw_Transmit call.
 * Pedal SWC reads 2 sensors per 10ms cycle = 2 Hw_Transmit calls.
 * Sensor-to-sensor delta = 11 (< plausibility threshold 819).
 * Cycle-to-cycle delta per sensor = 22 (>= stuck threshold 10).
 */
static uint16 spi_sim_angle = 400u;
static uint8  spi_sim_up    = 1u;

/* ---- Default center oscillation state (Channel 1+ / steering sensors) ---- */

/**
 * @brief  Simulated steering sensor default — center with oscillation
 *
 * Raw 8191 = 14-bit midpoint → 0° actual angle.
 * Oscillation ±40 raw gives ±0.2° — well within 5° plausibility threshold.
 * Without this, channels > 0 would use the pedal dead-zone (200-800) which
 * maps to -44° to -41° steering angle and instantly triggers a plausibility
 * fault against the 0° commanded position.
 */
static uint16 spi_steer_default_offset = 0u;
static uint8  spi_steer_default_up     = 1u;

/* ---- UDP pedal override state ---- */

static int    spi_udp_fd          = -1;      /**< UDP socket fd (-1=disabled)*/
static uint16 spi_override_target = 0xFFFFu; /**< Target angle or CLEAR     */
static uint16 spi_override_offset = 0u;      /**< Oscillation offset         */
static uint8  spi_override_up     = 1u;      /**< Oscillation direction      */

/* ---- Spi_Hw_* implementations ---- */

/**
 * @brief  Initialize SPI hardware (POSIX: set up UDP pedal override socket)
 * @return E_OK always
 *
 * @details If SPI_PEDAL_UDP_PORT env var is set, creates a non-blocking UDP
 *          socket bound to localhost:port.  Only the CVC container sets this
 *          env var; other ECU containers skip socket creation gracefully.
 */
Std_ReturnType Spi_Hw_Init(void)
{
    const char *port_str = getenv("SPI_PEDAL_UDP_PORT");

    if (port_str != NULL_PTR)
    {
        char *endptr = NULL_PTR;
        long port = strtol(port_str, &endptr, 10);

        if ((endptr != port_str) && (port > 0) && (port <= 65535))
        {
            int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

            if (fd >= 0)
            {
                struct sockaddr_in addr;
                (void)memset(&addr, 0, sizeof(addr));
                addr.sin_family      = AF_INET;
                addr.sin_port        = htons((uint16_t)port);
                addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

                if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
                {
                    fprintf(stderr, "[SPI] UDP bind(:%ld) failed\n", port);
                    (void)close(fd);
                }
                else
                {
                    spi_udp_fd = fd;
                    fprintf(stderr, "[SPI] Pedal override UDP on :%ld\n",
                            port);
                }
            }
        }
    }

    return E_OK;
}

/**
 * @brief  SPI transmit (POSIX: returns simulated AS5048A angle sensor data)
 * @param  Channel  SPI channel (ignored)
 * @param  TxBuf    Transmit buffer (ignored)
 * @param  RxBuf    Receive buffer — filled with simulated sensor response
 * @param  Length   Transfer length in words
 * @return E_OK always
 *
 * @details When UDP override is active, returns the target angle with
 *          oscillation (step 7, range +40) to satisfy Swc_Pedal
 *          stuck-detection and plausibility checks.
 *          When no override, oscillates 200-800 (dead zone = torque 0).
 */
Std_ReturnType Spi_Hw_Transmit(uint8 Channel, const uint16* TxBuf,
                                uint16* RxBuf, uint8 Length)
{
    (void)TxBuf;

    /* ---- Drain UDP socket for latest override command ---- */
    if (spi_udp_fd >= 0)
    {
        uint8 udp_buf[2];
        ssize_t n;

        while ((n = recv(spi_udp_fd, udp_buf, 2u, MSG_DONTWAIT)) == 2)
        {
            uint16 cmd = (uint16)udp_buf[0]
                       | ((uint16)udp_buf[1] << 8u);

            if (cmd == SPI_OVERRIDE_CLEAR)
            {
                spi_override_target = SPI_OVERRIDE_CLEAR;
                spi_override_offset = 0u;
                spi_override_up     = 1u;
            }
            else
            {
                spi_override_target = cmd & 0x3FFFu;
                spi_override_offset = 0u;
                spi_override_up     = 1u;
            }
        }
    }

    /* ---- Return simulated angle ---- */
    if ((RxBuf != NULL_PTR) && (Length > 0u))
    {
        if (spi_override_target != SPI_OVERRIDE_CLEAR)
        {
            /* UDP pedal override active — oscillate around target angle */
            uint32 angle = (uint32)spi_override_target
                         + (uint32)spi_override_offset;

            if (angle > 0x3FFFu)
            {
                angle = 0x3FFFu;
            }
            RxBuf[0] = (uint16)(angle & 0x3FFFu);

            /* Advance oscillation to pass stuck detection */
            if (spi_override_up != 0u)
            {
                spi_override_offset += SPI_OVERRIDE_STEP;
                if (spi_override_offset > SPI_OVERRIDE_RANGE)
                {
                    spi_override_up = 0u;
                }
            }
            else
            {
                if (spi_override_offset >= SPI_OVERRIDE_STEP)
                {
                    spi_override_offset -= SPI_OVERRIDE_STEP;
                }
                else
                {
                    spi_override_offset = 0u;
                    spi_override_up     = 1u;
                }
            }
        }
        else if (Channel == 0u)
        {
            /* Channel 0 (pedal): dead-zone oscillation (200-800).
             * Maps to pedal position < 67 → torque = 0, vehicle stays still. */
            RxBuf[0] = spi_sim_angle & 0x3FFFu;

            if (spi_sim_up != 0u)
            {
                spi_sim_angle += 7u;
                if (spi_sim_angle > 800u)
                {
                    spi_sim_up = 0u;
                }
            }
            else
            {
                spi_sim_angle -= 7u;
                if (spi_sim_angle < 200u)
                {
                    spi_sim_up = 1u;
                }
            }
        }
        else
        {
            /* Channel 1+ (steering): center position with oscillation.
             * Raw 8191 = 14-bit midpoint → 0° actual angle.
             * Oscillation ±40 raw → ±0.2° (within 5° plausibility threshold).
             * Prevents false steering plausibility fault from pedal dead-zone
             * values (200-800 → -44°) that would fire on every boot. */
            uint16 center = 8191u;
            RxBuf[0] = (uint16)((center + spi_steer_default_offset) & 0x3FFFu);

            if (spi_steer_default_up != 0u)
            {
                spi_steer_default_offset += SPI_OVERRIDE_STEP;
                if (spi_steer_default_offset > SPI_OVERRIDE_RANGE)
                {
                    spi_steer_default_up = 0u;
                }
            }
            else
            {
                if (spi_steer_default_offset >= SPI_OVERRIDE_STEP)
                {
                    spi_steer_default_offset -= SPI_OVERRIDE_STEP;
                }
                else
                {
                    spi_steer_default_offset = 0u;
                    spi_steer_default_up     = 1u;
                }
            }
        }
    }

    return E_OK;
}

/**
 * @brief  Get SPI status (POSIX: always idle)
 * @return SPI_IDLE (1)
 */
uint8 Spi_Hw_GetStatus(void)
{
    return 1u; /* SPI_IDLE */
}
