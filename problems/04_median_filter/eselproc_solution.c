#include "stdint.h"
#include "eselproc_drv.h"

#include <stdint.h>
#include <stdbool.h>

static inline void swap_u8(uint8_t *a, uint8_t *b) {
    if (*a > *b) {
        uint8_t t = *a;
        *a = *b;
        *b = t;
    }
}

// solution not really optimized for streaming yet.
static inline uint8_t median5_u8(uint8_t x[5]) {
    uint8_t a = x[0];
    uint8_t b = x[1];
    uint8_t c = x[2]; // << values will be sorted such that c is median
    uint8_t d = x[3];
    uint8_t e = x[4];

    // partial sorting network for median of 5
    //swap_u8(&a, &b);
    //swap_u8(&d, &e);
    //swap_u8(&a, &c);
    //swap_u8(&b, &c);
    //swap_u8(&a, &d);
    //swap_u8(&c, &d);
    //swap_u8(&b, &e);
    //swap_u8(&b, &c);
    // faster 7 compares:
    swap_u8(&a, &b);
    swap_u8(&d, &e);
    swap_u8(&a, &d);
    swap_u8(&b, &e);
    swap_u8(&b, &c);
    swap_u8(&c, &d);
    swap_u8(&b, &c);

    // c is now the median
    return c;
}

#define SORT2U8BRANCH(a, b) { \
    uint8_t x = (a);  \
    uint8_t y = (b);  \
    if (x > y) {      \
        (a) = y;      \
        (b) = x;      \
    }                 \
}

#define SORT2U32BRANCH(a, b) { \
    uint32_t x = (a);          \
    uint32_t y = (b);          \
    if (x > y) {               \
        (a) = y;               \
        (b) = x;               \
    }                          \
}

#define SORT2B(a, b) {                 \
    uint8_t x = (a);                   \
    uint8_t y = (b);                   \
    uint8_t m = 0u - (uint8_t)(x > y); \
    uint8_t t = (x ^ y) & m;           \
    (a) = x ^ t;                       \
    (b) = y ^ t;                       \
}

// solution not really optimized for streaming yet.
static inline uint8_t median5_u8packed(uint32_t abcd, uint8_t next) {
    uint8_t a = (uint8_t)abcd;
    uint8_t b = (uint8_t)(abcd >> 8);
    uint8_t c = (uint8_t)(abcd >> 16);
    uint8_t d = (uint8_t)(abcd >> 24);

    uint8_t e = next;

    // partial sorting network for median of 5
    // faster 7 compares:
    SORT2U8BRANCH(a, b);
    SORT2U8BRANCH(d, e);
    SORT2U8BRANCH(a, d);
    SORT2U8BRANCH(b, e);
    SORT2U8BRANCH(b, c);
    SORT2U8BRANCH(c, d);
    SORT2U8BRANCH(b, c);

    // c is now the median
    return c;
}

static inline uint32_t median5_u32packed(uint32_t abcd, uint32_t next) {
    uint32_t a =  abcd        & 0xffu;
    uint32_t b = (abcd >>  8) & 0xffu;
    uint32_t c = (abcd >> 16) & 0xffu;
    uint32_t d =  abcd >> 24;
    uint32_t e =  next;

    // partial sorting network for median of 5
    // faster 7 compares:
    SORT2U32BRANCH(a, b);
    SORT2U32BRANCH(d, e);
    SORT2U32BRANCH(a, d);
    SORT2U32BRANCH(b, e);
    SORT2U32BRANCH(b, c);
    SORT2U32BRANCH(c, d);
    SORT2U32BRANCH(b, c);

    // c is now the median
    return c;
}

#define LIF_FIFO_DEPTH 1

void eselproc_solve_loop_fullmedian(void) {
  uint8_t x[5];
  int8_t w = 0;
  int out_fifo_cnt = 0;
  uint8_t emit = 0;
  uint8_t i_payload_stream_fragment;
  uint32_t i_payload_stream_last;
  uint32_t o_packed_payload;

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();
  // newest version of LIF does not clear flush control bit, so non-empty out fifo is immediately emptied.
  eselproc_lif_control_request_flush();

  // Ask hardware to accept exactly N input samples:
  // o_ready will stay high for all N samples
  while(1)
  {
      // sample exactly one next element now
    eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);

    while(1)
    {
      // Wait until one sampled input vector is present in the input FIFO
      //eselproc_lif_try_pop_i_payload();
      if (!eselproc_lif_try_pop_i_payload())
        break;
      // Pop one vector into the input shadow registers
      // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide.
      uint32_t o_packed_payload = eselproc_lif_read_i_payload(0);
      // unpack input
      uint8_t i_payload_stream_fragment = (uint8_t) o_packed_payload;
      uint32_t i_payload_stream_last = o_packed_payload & 0x100;
      // update window
      x[w] = i_payload_stream_fragment;
      //w = (w + 1) % 5; modulo is super expensive on rv32i (no m)
      //if(emit || !w)
      if(++w >= 5)
      {
        w = 0;
        emit = 1;
      }
      if(emit)
      {
      //  emit = 1;
        //compute median of current window:
        uint8_t m =  median5_u8(x);
        eselproc_lif_write_o_payload(0, i_payload_stream_last | m);
        eselproc_lif_push_o_payload();
        if(i_payload_stream_last)
        {
          w = 0;
          emit = 0;
          break;
        }
      }
    }
  }
}

void eselproc_solve_loop_fullmedian_packed(void) {
  uint32_t x;
  uint32_t cntr = 0;
  uint32_t o_packed_payload;

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();
  // newest version of LIF does not clear flush control bit, so non-empty out fifo is immediately emptied.
  eselproc_lif_control_request_flush();

  // Ask hardware to accept exactly N input samples:
  // o_ready will stay high for all N samples
  while(1)
  {
      // sample exactly one next element now
    eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);

    while(eselproc_lif_try_pop_i_payload()) // if new data is in, get it.
    {
      // Pop one vector into the input shadow registers
      // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide.
      uint32_t o_packed_payload = eselproc_lif_read_i_payload(0);
      // unpack input
      uint32_t i_payload_stream_fragment = o_packed_payload & 0x0ffu;
      if(++cntr >= 5)
      {
        uint32_t i_payload_stream_last = o_packed_payload & 0x100;
        //compute median of current window:
        uint32_t m =  median5_u32packed(x, i_payload_stream_fragment);
        eselproc_lif_write_o_payload(0, i_payload_stream_last | m);
        eselproc_lif_push_o_payload();
        if(i_payload_stream_last)
        {
          cntr = 0;
          break;
        }
      }      
      // update window
      x = (x << 8) | i_payload_stream_fragment;
    }
  }
}

typedef struct {
    uint8_t v[5];    // current values by ring-buffer slot
    uint8_t ord[5];  // slot ids sorted by v[]
    uint8_t pos[5];  // inverse map: pos[slot] -> index in ord[]
} median5_state_t;

static inline uint8_t median5_init(median5_state_t *s, const uint8_t x[5]) {
    for (int i = 0; i < 5; ++i) {
        s->v[i] = x[i];
        s->ord[i] = (uint8_t)i;
    }

    for (int i = 1; i < 5; ++i) {
        uint8_t slot = s->ord[i];
        uint8_t val  = s->v[slot];
        int j = i;
        while (j > 0 && val < s->v[s->ord[j - 1]]) {
            s->ord[j] = s->ord[j - 1];
            --j;
        }
        s->ord[j] = slot;
    }

    for (int i = 0; i < 5; ++i) {
        s->pos[s->ord[i]] = (uint8_t)i;
    }

    return s->v[s->ord[2]];
}

static inline uint8_t median5_update(median5_state_t *s, uint8_t slot, uint8_t newv) {
    int p = s->pos[slot];
    s->v[slot] = newv;

    while (p > 0 && newv < s->v[s->ord[p - 1]]) {
        uint8_t other = s->ord[p - 1];
        s->ord[p] = other;
        s->pos[other] = (uint8_t)p;
        --p;
    }

    while (p < 4 && newv > s->v[s->ord[p + 1]]) {
        uint8_t other = s->ord[p + 1];
        s->ord[p] = other;
        s->pos[other] = (uint8_t)p;
        ++p;
    }

    s->ord[p] = slot;
    s->pos[slot] = (uint8_t)p;
    return s->v[s->ord[2]];
}

void eselproc_solve_loop_updatemedian(void) {
  uint8_t x[5];
  int8_t w = 0;
  int out_fifo_cnt = 0;
  median5_state_t s;
  uint8_t emit = 0;
  uint8_t i_payload_stream_fragment;
  uint32_t i_payload_stream_last;
  uint32_t o_packed_payload;

  // Optional cleanup from previous transaction
  eselproc_lif_control_reset();
  eselproc_lif_clear_fifos();
  // newest version of LIF does not clear flush control bit, so non-empty out fifo is immediately emptied.
  eselproc_lif_control_request_flush();

  // Ask hardware to accept exactly N input samples:
  // o_ready will stay high for all N samples
  while(1)
  {
      // sample exactly one next element now
    eselproc_lif_control_set_sample_count(LIF_FIFO_DEPTH);
    while(1)
    {
      // Wait until one sampled input vector is present in the input FIFO
      if (!eselproc_lif_try_pop_i_payload())
        break;
      // Pop one vector into the input shadow registers
      // the eselproc-lif is generic. It offers synchronous FIFO-Lanes, each 32-bit wide.
      o_packed_payload = eselproc_lif_read_i_payload(0);
      i_payload_stream_fragment = (uint8_t) o_packed_payload;
      i_payload_stream_last = o_packed_payload & 0x100;
      if(!emit)
      {
        x[w] = i_payload_stream_fragment;
        w = (w + 1) % 5;
        if(w == 0)  
        {
          emit = 1;
          uint32_t packed_out = (uint32_t) median5_init(&s, x);
          packed_out |= i_payload_stream_last;
          eselproc_lif_write_o_payload(0, packed_out);
          eselproc_lif_push_o_payload();
        }
      }
      else
      {
        //compute median of current window:
        uint32_t packed_out = (uint32_t) median5_update(&s, w, i_payload_stream_fragment);
        packed_out |= i_payload_stream_last;
        w = (w + 1) % 5;
        eselproc_lif_write_o_payload(0, packed_out);
        eselproc_lif_push_o_payload();
      }
      // check if we are done with this stream
      if(i_payload_stream_last == 0x100)
      {
        emit = 0;
        w = 0;
        //continue; // we are done and eselproc will launch a new eselproc_solve_loop() from main
      }
    }
  }
}

void eselproc_solve_loop(void)
{
  //eselproc_solve_loop_fullmedian(); // 409,176 with modulo: 612,155
  eselproc_solve_loop_fullmedian_packed(); // 364,107
  //eselproc_solve_loop_updatemedian(); // 790,933
}
