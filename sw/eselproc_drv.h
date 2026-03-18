#ifndef __INCLUDE_ESELPROC_DRV_H__
#define __INCLUDE_ESELPROC_DRV_H__

#include <stdint.h>
#include <stdbool.h>

#define ROM_BASE 0x0

#define RAM_BASE 0x10000
#define RAM_SIZE 0x400
#define RAM_END  (RAM_BASE + RAM_SIZE - 1)

#define ESELPROC_LATCHUP_IF_BASE 0x20000
#define ESELPROC_LATCHUP_IF_SIZE 0x400
#define ESELPROC_LATCHUP_IF_END  (ESELPROC_LATCHUP_IF_BASE + ESELPROC_LATCHUP_IF_SIZE - 1)

/* Keep these in sync with the RTL instance visible to software. */
#ifndef ESELPROC_LATCHUP_IF_N_IN_W_PAYLOADS
#define ESELPROC_LATCHUP_IF_N_IN_W_PAYLOADS  1u
#endif

#ifndef ESELPROC_LATCHUP_IF_N_OUT_W_PAYLOADS
#define ESELPROC_LATCHUP_IF_N_OUT_W_PAYLOADS 1u
#endif

/* Register map */
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

/* STATUS register bits */
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

/* CONTROL register bits */
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

/* Raw register access */
uint32_t eselproc_lif_read_status(void);
uint32_t eselproc_lif_read_control(void);
void eselproc_lif_write_control(uint32_t val);
void eselproc_lif_setmask_control(uint32_t mask);
void eselproc_lif_resetmask_control(uint32_t mask);

/* Status helpers */
bool eselproc_lif_status_in_empty(void);
bool eselproc_lif_status_in_full(void);
bool eselproc_lif_status_out_empty(void);
bool eselproc_lif_status_out_full(void);
bool eselproc_lif_status_flushing(void);
bool eselproc_lif_status_sample_count_zero(void);
uint32_t eselproc_lif_status_in_level(void);
uint32_t eselproc_lif_status_out_level(void);
bool eselproc_lif_status_in_nonempty(void);

/* Control helpers */
void eselproc_lif_control_set_sample_count(uint16_t sample_count);
uint16_t eselproc_lif_control_get_sample_count(void);
void eselproc_lif_control_request_flush(void);
void eselproc_lif_control_clear_in_fifo(void);
void eselproc_lif_control_clear_out_fifo(void);
void eselproc_lif_control_reset(void);

/* Input FIFO access */
uint32_t eselproc_lif_pop_i_payload(void);
bool eselproc_lif_try_pop_i_payload(void);
uint32_t eselproc_lif_read_i_payload(uint32_t idx);
uint32_t eselproc_lif_read_i_payload_word(void);
uint16_t eselproc_lif_read_i_payload_a16(void);
uint16_t eselproc_lif_read_i_payload_b16(void);

/* Output staging + push */
uint32_t eselproc_lif_read_o_payload(uint32_t idx);
void eselproc_lif_write_o_payload(uint32_t idx, uint32_t val);
void eselproc_lif_push_o_payload(void);
void eselproc_lif_write_o_payload_word(uint32_t val);
void eselproc_lif_write_o_payload_a16b16(uint16_t a, uint16_t b);

/* Convenience helpers */
void eselproc_lif_clear_fifos(void);
bool eselproc_lif_output_can_push(void);
bool eselproc_lif_input_can_pop(void);

#endif