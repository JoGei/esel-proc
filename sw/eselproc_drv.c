#include "eselproc_drv.h"
#include "stdint.h"
#include "stdbool.h"

#define ROM_BASE 0x0

#define RAM_BASE 0x10000
#define RAM_SIZE 0x400
#define RAM_END  (RAM_BASE + RAM_SIZE - 1)

#define ESELPROC_LATCHUP_IF_BASE 0x20000
#define ESELPROC_LATCHUP_IF_SIZE 0x400
#define ESELPROC_LATCHUP_IF_END  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_SIZE - 1)

/* -------------------------------------------------------------------------- */
/* Configuration                                                               */
/* -------------------------------------------------------------------------- */
/* Keep these in sync with the instantiated RTL parameters as seen by SW. */

// TODO: We could generate a parameter header from this RTL to the generic latchup interface
#ifndef ESELPROC_LATCHUP_IF_N_IN_W_PAYLOADS
#define ESELPROC_LATCHUP_IF_N_IN_W_PAYLOADS  1u
#endif

#ifndef ESELPROC_LATCHUP_IF_N_OUT_W_PAYLOADS
#define ESELPROC_LATCHUP_IF_N_OUT_W_PAYLOADS 1u
#endif


// Register map
#define ESELPROC_LATCHUP_IF_STATUS_REG_OFFSET       0x00u
#define ESELPROC_LATCHUP_IF_CONTROL_REG_OFFSET      0x04u
#define ESELPROC_LATCHUP_IF_IN_POP_REG_OFFSET       0x08u
#define ESELPROC_LATCHUP_IF_IN_DATA_BASE_OFFSET     0x0Cu
#define ESELPROC_LATCHUP_IF_OUT_DATA_BASE_OFFSET    0x40u
#define ESELPROC_LATCHUP_IF_OUT_PUSH_REG_OFFSET     0x80u

#define ESELPROC_LATCHUP_IF_STATUS_REG_ADDR \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_STATUS_REG_OFFSET)
#define ESELPROC_LATCHUP_IF_CONTROL_REG_ADDR \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_CONTROL_REG_OFFSET)
#define ESELPROC_LATCHUP_IF_IN_POP_REG_ADDR \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_IN_POP_REG_OFFSET)
#define ESELPROC_LATCHUP_IF_OUT_PUSH_REG_ADDR \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_OUT_PUSH_REG_OFFSET)

#define ESELPROC_LATCHUP_IF_IN_DATA_ADDR(idx) \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_IN_DATA_BASE_OFFSET + (4u * (uint32_t)(idx)))

#define ESELPROC_LATCHUP_IF_OUT_DATA_ADDR(idx) \
  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_OUT_DATA_BASE_OFFSET + (4u * (uint32_t)(idx)))

/* -------------------------------------------------------------------------- */
/* STATUS register bits                                                        */
/* -------------------------------------------------------------------------- */
/* RTL:
 * [0]     in_empty
 * [1]     in_full
 * [2]     out_empty
 * [3]     out_full
 * [4]     flushing
 * [5]     sample_count_zero
 * [15:8]  in_level
 * [23:16] out_level
 */

#define ESELPROC_LATCHUP_IF_STATUS_IN_EMPTY_BIT            0u
#define ESELPROC_LATCHUP_IF_STATUS_IN_FULL_BIT             1u
#define ESELPROC_LATCHUP_IF_STATUS_OUT_EMPTY_BIT           2u
#define ESELPROC_LATCHUP_IF_STATUS_OUT_FULL_BIT            3u
#define ESELPROC_LATCHUP_IF_STATUS_FLUSHING_BIT            4u
#define ESELPROC_LATCHUP_IF_STATUS_SAMPLE_COUNT_ZERO_BIT   5u

#define ESELPROC_LATCHUP_IF_STATUS_IN_EMPTY_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_IN_EMPTY_BIT)
#define ESELPROC_LATCHUP_IF_STATUS_IN_FULL_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_IN_FULL_BIT)
#define ESELPROC_LATCHUP_IF_STATUS_OUT_EMPTY_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_OUT_EMPTY_BIT)
#define ESELPROC_LATCHUP_IF_STATUS_OUT_FULL_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_OUT_FULL_BIT)
#define ESELPROC_LATCHUP_IF_STATUS_FLUSHING_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_FLUSHING_BIT)
#define ESELPROC_LATCHUP_IF_STATUS_SAMPLE_COUNT_ZERO_MASK \
  (1u << ESELPROC_LATCHUP_IF_STATUS_SAMPLE_COUNT_ZERO_BIT)

#define ESELPROC_LATCHUP_IF_STATUS_IN_LEVEL_SHIFT  8u
#define ESELPROC_LATCHUP_IF_STATUS_IN_LEVEL_MASK   (0xFFu << ESELPROC_LATCHUP_IF_STATUS_IN_LEVEL_SHIFT)

#define ESELPROC_LATCHUP_IF_STATUS_OUT_LEVEL_SHIFT 16u
#define ESELPROC_LATCHUP_IF_STATUS_OUT_LEVEL_MASK  (0xFFu << ESELPROC_LATCHUP_IF_STATUS_OUT_LEVEL_SHIFT)

/* -------------------------------------------------------------------------- */
/* CONTROL register bits                                                       */
/* -------------------------------------------------------------------------- */
/* RTL:
 * [15:0]  sample_count_q
 * [16]    flush_q
 * [17]    clr_in_fifo
 * [18]    clr_out_fifo
 */

#define ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_SHIFT 0u
#define ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_MASK  0x0000FFFFu

#define ESELPROC_LATCHUP_IF_CONTROL_FLUSH_BIT          16u
#define ESELPROC_LATCHUP_IF_CONTROL_CLR_IN_FIFO_BIT    17u
#define ESELPROC_LATCHUP_IF_CONTROL_CLR_OUT_FIFO_BIT   18u

#define ESELPROC_LATCHUP_IF_CONTROL_FLUSH_MASK \
  (1u << ESELPROC_LATCHUP_IF_CONTROL_FLUSH_BIT)
#define ESELPROC_LATCHUP_IF_CONTROL_CLR_IN_FIFO_MASK \
  (1u << ESELPROC_LATCHUP_IF_CONTROL_CLR_IN_FIFO_BIT)
#define ESELPROC_LATCHUP_IF_CONTROL_CLR_OUT_FIFO_MASK \
  (1u << ESELPROC_LATCHUP_IF_CONTROL_CLR_OUT_FIFO_BIT)

// MMIO helpers
void mmio_write32(uint32_t addr, uint32_t val)
{
  *(volatile uint32_t *)addr = val;
}

uint32_t mmio_read32(uint32_t addr)
{
  return *(volatile uint32_t const *)addr;
}

// Raw register access
uint32_t eselproc_lif_read_status(void)
{
  return mmio_read32(ESELPROC_LATCHUP_IF_STATUS_REG_ADDR);
}

uint32_t eselproc_lif_read_control(void)
{
  return mmio_read32(ESELPROC_LATCHUP_IF_CONTROL_REG_ADDR);
}

void eselproc_lif_write_control(uint32_t val)
{
  mmio_write32(ESELPROC_LATCHUP_IF_CONTROL_REG_ADDR, val);
}

void eselproc_lif_setmask_control(uint32_t mask)
{
  eselproc_lif_write_control(eselproc_lif_read_control() | mask);
}

void eselproc_lif_resetmask_control(uint32_t mask)
{
  eselproc_lif_write_control(eselproc_lif_read_control() & ~mask);
}

// Status helpers
bool eselproc_lif_status_in_empty(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_IN_EMPTY_MASK) != 0u;
}

bool eselproc_lif_status_in_full(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_IN_FULL_MASK) != 0u;
}

bool eselproc_lif_status_out_empty(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_OUT_EMPTY_MASK) != 0u;
}

bool eselproc_lif_status_out_full(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_OUT_FULL_MASK) != 0u;
}

bool eselproc_lif_status_flushing(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_FLUSHING_MASK) != 0u;
}

bool eselproc_lif_status_sample_count_zero(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_SAMPLE_COUNT_ZERO_MASK) != 0u;
}

uint32_t eselproc_lif_status_in_level(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_IN_LEVEL_MASK)
       >> ESELPROC_LATCHUP_IF_STATUS_IN_LEVEL_SHIFT;
}

uint32_t eselproc_lif_status_out_level(void)
{
  return (eselproc_lif_read_status() & ESELPROC_LATCHUP_IF_STATUS_OUT_LEVEL_MASK)
       >> ESELPROC_LATCHUP_IF_STATUS_OUT_LEVEL_SHIFT;
}

// Convenience alias
bool eselproc_lif_status_in_nonempty(void)
{
  return !eselproc_lif_status_in_empty();
}

// Control helpers
void eselproc_lif_control_set_sample_count(uint16_t sample_count)
{
  uint32_t ctrl = eselproc_lif_read_control();
  ctrl &= ~ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_MASK;
  ctrl |= ((uint32_t)sample_count << ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_SHIFT)
       & ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_MASK;
  eselproc_lif_write_control(ctrl);
}

uint16_t eselproc_lif_control_get_sample_count(void)
{
  return (uint16_t)((eselproc_lif_read_control() & ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_MASK)
         >> ESELPROC_LATCHUP_IF_CONTROL_SAMPLE_COUNT_SHIFT);
}

void eselproc_lif_control_request_flush(void)
{
  eselproc_lif_setmask_control(ESELPROC_LATCHUP_IF_CONTROL_FLUSH_MASK);
}

void eselproc_lif_control_clear_in_fifo(void)
{
  eselproc_lif_setmask_control(ESELPROC_LATCHUP_IF_CONTROL_CLR_IN_FIFO_MASK);
}

void eselproc_lif_control_clear_out_fifo(void)
{
  eselproc_lif_setmask_control(ESELPROC_LATCHUP_IF_CONTROL_CLR_OUT_FIFO_MASK);
}

void eselproc_lif_control_reset(void)
{
  eselproc_lif_write_control(0u);
}

// Input FIFO access
// IN_POP returns 1 if one vector was popped into the input shadow registers,
// otherwise 0 if input FIFO was empty.
uint32_t eselproc_lif_pop_i_payload(void)
{
  return mmio_read32(ESELPROC_LATCHUP_IF_IN_POP_REG_ADDR);
}

bool eselproc_lif_try_pop_i_payload(void)
{
  return eselproc_lif_pop_i_payload() != 0u;
}

uint32_t eselproc_lif_read_i_payload(uint32_t idx)
{
  return mmio_read32(ESELPROC_LATCHUP_IF_IN_DATA_ADDR(idx));
}

// Output staging + push
uint32_t eselproc_lif_read_o_payload(uint32_t idx)
{
  return mmio_read32(ESELPROC_LATCHUP_IF_OUT_DATA_ADDR(idx));
}

void eselproc_lif_write_o_payload(uint32_t idx, uint32_t val)
{
  mmio_write32(ESELPROC_LATCHUP_IF_OUT_DATA_ADDR(idx), val);
}

void eselproc_lif_push_o_payload(void)
{
  mmio_write32(ESELPROC_LATCHUP_IF_OUT_PUSH_REG_ADDR, 1u);
}

// Common N_OUT_W_PAYLOADS=1 helper
void eselproc_lif_write_o_payload_word(uint32_t val)
{
  eselproc_lif_write_o_payload(0u, val);
}

void eselproc_lif_write_o_payload_a16b16(uint16_t a, uint16_t b)
{
  eselproc_lif_write_o_payload(0u, ((uint32_t)b << 16) | (uint32_t)a);
}

// Higher-level convenience helpers
 void eselproc_lif_clear_fifos(void)
{
  eselproc_lif_control_clear_in_fifo();
  eselproc_lif_control_clear_out_fifo();
}

bool eselproc_lif_output_can_push(void)
{
  return !eselproc_lif_status_out_full();
}

bool eselproc_lif_input_can_pop(void)
{
  return !eselproc_lif_status_in_empty();
}
