#include "stdint.h"
#include "eselproc_drv.h"

#include <stdbool.h>
#include <stdlib.h>

#define LIF_FIFO_DEPTH 1

static void print_pixel(uint32_t x, uint32_t y, bool last)
{
    uint32_t packed_out = ((uint32_t)last << 16) | ((y & 0x0ffu) << 8) | (x & 0x0ffu);
    eselproc_lif_write_o_payload(0, packed_out);
    eselproc_lif_push_o_payload();
}

void eselproc_solve_loop(void)
{
    // Optional cleanup from previous transaction
    eselproc_lif_control_reset();
    eselproc_lif_clear_fifos();
    // newest version of LIF does not clear flush control bit, so non-empty out fifo is immediately emptied.
    eselproc_lif_control_request_flush();

    while (1)
    {
        // allow sampling as many elemts, as the FIFO can hold
        eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);

        while (eselproc_lif_try_pop_i_payload()) // if new data is in, get it.
        {
            // Pop one vector into the input shadow registers
            // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide.
            uint32_t o_packed_payload = eselproc_lif_read_i_payload(0);
            // unpack input
            int32_t x0 = (int8_t)(o_packed_payload & 0x0ffu);
            int32_t y0 = (int8_t)((o_packed_payload >> 8) & 0x0ffu);
            int32_t x1 = (int8_t)((o_packed_payload >> 16) & 0x0ffu);
            int32_t y1 = (int8_t)((o_packed_payload >> 24) & 0x0ffu);

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            int32_t sx = (dx > 0) - (dx < 0); // sign of dx
            int32_t sy = (dy > 0) - (dy < 0); // sign of dy

            int32_t adx = dx >= 0 ? dx : -dx;
            int32_t ady = dy >= 0 ? dy : -dy;

            int32_t x = x0;
            int32_t y = y0;

            int32_t err = 0;

            print_pixel(x, y, dx == 0 && dy == 0);

            if (adx >= ady)
            {
                int32_t err = 0;

                for (int i = 0; i < adx; i++)
                {
                    x += sx;
                    err += ady;

                    // Strict crossing of midpoint:
                    // move in Y only when err * 2 > adx
                    if ((err << 1) > adx)
                    {
                        y += sy;
                        err -= adx;
                    }

                    print_pixel(x, y, i == (adx - 1));
                }
            }
            else
            {
                // Y is the major axis
                int32_t err = 0;

                for (int i = 0; i < ady; i++)
                {
                    y += sy;
                    err += adx;

                    if ((err << 1) > ady)
                    {
                        x += sx;
                        err -= ady;
                    }

                    print_pixel(x, y, i == (ady - 1));
                }
            }
        }
    }
}
