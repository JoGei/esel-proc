#include "stdint.h"
#include "eselproc_drv.h"
#include <math.h> //< feels like cheating.

// integer squareroot: https://en.wikipedia.org/wiki/Integer_square_root
// https://web.archive.org/web/20120306040058/http://medialab.freaknet.org/martin/src/sqrt/sqrt.c
uint16_t usqrt32(uint32_t x) {
    uint32_t res = 0;
    uint32_t bit = 1u << 30;

    while (bit > x) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (x >= res + bit) {
            x  -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    return (uint16_t)res;
}

#define LIF_FIFO_DEPTH 1

void eselproc_solve_loop(void) {

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();

  // Ask hardware to accept exactly one input sample
  eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);

  // Wait until one sampled input vector is present in the input FIFO
  while (!eselproc_lif_input_can_pop()) { }

  do {
    // Pop one vector into the input shadow registers
    while (!eselproc_lif_try_pop_i_payload()) { }

    // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide. 
    // In the multiplier example we have a single 32-bit output FIFO and a single 32-bit
    // input FIFO. The latter packs the two 16-bit payloads together {b, a} (hi, lo)
    // lets unpack them.
    uint32_t xx = eselproc_lif_read_i_payload(0);

    uint16_t x = usqrt32(xx);

    // Wait until output FIFO has space
    while (!eselproc_lif_output_can_push()) {
    }

    // Stage one output vector and push it into the output FIFO
    eselproc_lif_write_o_payload_word(x);
    eselproc_lif_push_o_payload();

    // Start emitting output FIFO contents
    eselproc_lif_control_request_flush();
  } while(eselproc_lif_input_can_pop());

}
