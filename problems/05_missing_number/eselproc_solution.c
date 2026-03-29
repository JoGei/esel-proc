#include "stdint.h"
#include "eselproc_drv.h"

#include <stdint.h>
#include <stdbool.h>

#define LIF_FIFO_DEPTH 1

void eselproc_solve_loop(void) {
  int8_t cnt = 0;
  uint16_t missing_number = 0;
  uint32_t i_payload_stream_last_bit = 0;

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();
  // newest version of LIF does not clear flush control bit, so non-empty out fifo is immediately emptied.
  eselproc_lif_control_request_flush();

  while(1)
  {
    // allow sampling as many elemts, as the FIFO can hold
    eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);

    while(eselproc_lif_try_pop_i_payload()) // if new data is in, get it.
    {
      // Pop one vector into the input shadow registers
      // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide.
      uint32_t o_packed_payload = eselproc_lif_read_i_payload(0);
      // unpack input
      uint32_t i_packed = eselproc_lif_read_i_payload(0);

      uint32_t i_payload_stream_last_bit = i_packed & 0x010000;
      missing_number ^= (uint16_t)i_packed ^ cnt++;

      if(i_payload_stream_last_bit)
      {
        eselproc_lif_write_o_payload(0, missing_number ^ cnt);
        eselproc_lif_push_o_payload();
        missing_number = 0;
        cnt = 0;
        break;        
      }
    }
  }
}
