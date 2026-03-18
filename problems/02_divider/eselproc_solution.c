#include "stdint.h"
#include "eselproc_drv.h"

void eselproc_solve_loop(void) {

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();

  // Ask hardware to accept exactly one input sample
  eselproc_lif_control_set_sample_count(16);

  // Wait until one sampled input vector is present in the input FIFO
  while (!eselproc_lif_input_can_pop()) {
  }
  do {
    // Pop one vector into the input shadow registers
    while (!eselproc_lif_try_pop_i_payload()) {
    }

    // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide. 
    // In the multiplier example we have a single 32-bit output FIFO and a single 32-bit
    // input FIFO. The latter packs the two 16-bit payloads together {b, a} (hi, lo)
    // lets unpack them.
    uint32_t a = eselproc_lif_read_i_payload(0);
    uint32_t b = eselproc_lif_read_i_payload(1);

    uint32_t quotient = b ? a / b : ~0;
    uint32_t remainder = b ? a % b : ~0;

    // Wait until output FIFO has space
    while (!eselproc_lif_output_can_push()) {
    }

    // Stage one output vector and push it into the output FIFO
    eselproc_lif_write_o_payload(0, quotient);
    eselproc_lif_write_o_payload(1, remainder);
    eselproc_lif_push_o_payload();

    // Start emitting output FIFO contents
    eselproc_lif_control_request_flush();
  } while(eselproc_lif_input_can_pop());

}
