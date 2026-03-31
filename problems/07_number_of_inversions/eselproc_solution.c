#include "stdint.h"
#include "eselproc_drv.h"

#include <stdbool.h>
#include <stdlib.h>

#define LIF_FIFO_DEPTH 1

#define MAX_K 0xf // given by the problems testbench. k is a 4 bit unsigned type so 0xF. Would be way harder if that was not the case.
// Solution alongside CSES blog post: https://codeforces.com/blog/entry/142894
// Since we have a fixed window size, we can just maintain the inversion count of a window.
// Once a window is full and a new element arrives, we compute the leaving elements contribution to the inversion count
// and subtract it from our stateful inversion count -> see 1)
// Then, we add the new element -> see 2) and compute its contributions -> see 3). We add that contribution to our inv
// count state implemented with a simple MAX_K ringbuffer. Used size is dynamic by given k. Updates in 4)
typedef struct
{
    uint8_t k_;          ///< configured window size
    uint8_t size_;       ///< current number of elements
    uint8_t head_;       ///< oldest element index
    uint16_t inv_count_; ///< max for k=15 is 15*14/2 = 105
    int32_t buf_[MAX_K];
} inv_window_t; ///< the stateful window

static bool invwin_init(inv_window_t *w, uint8_t k)
{
    uint8_t i;

    if (w == 0 || k == 0 || k > MAX_K)
    {
        return false;
    }

    w->k_ = k;
    w->size_ = 0;
    w->head_ = 0;
    w->inv_count_ = 0;

    for (i = 0; i < MAX_K; i++)
    {
        w->buf_[i] = 0;
    }

    return true;
}

static uint8_t next_index(uint8_t idx, uint8_t k)
{
    idx++;
    if (idx == k)
    {
        idx = 0;
    }
    return idx;
}

// Push one value from the stream.
// Returns true when a full window exists and writes the current inversion count.
static bool invwin_push(inv_window_t *w, int32_t x, uint8_t *out_count)
{
    if (w->size_ < w->k_)
    {
        // Filling phase: append at logical end
        int32_t add = 0;

        int idx = w->head_;
        for (int i = 0; i < w->size_; i++)
        {
            if (w->buf_[idx] > x)
            {
                add++;
            }
            idx++;
            if (idx == w->k_)
            {
                idx = 0;
            }
        }

        // physical index of append position
        idx = w->head_ + w->size_;
        if (idx >= w->k_)
        {
            idx -= w->k_; // no modulo
        }

        w->buf_[idx] = x;
        w->size_++;
        w->inv_count_ += add;

        if (w->size_ == w->k_)
        {
            if (out_count != 0)
            {
                *out_count = w->inv_count_;
            }
            return true; // first filled-up window here
        }
        return false; // since we are not full yet
    }
    else
    {
        // Full window case
        // 1) subtract contribution of outgoing oldest element
        int32_t old = w->buf_[w->head_];
        int32_t sub = 0;

        int idx = next_index(w->head_, w->k_);
        for (int i = 1; i < w->k_; i++)
        {
            if (old > w->buf_[idx])
            {
                sub++;
            }
            idx++;
            if (idx == w->k_)
            {
                idx = 0;
            }
        }

        w->inv_count_ -= sub;
        // 2) overwrite oldest slot with new value
        w->buf_[w->head_] = x;

        // 3) add contribution of new element as newest element
        // Remaining window elements are all except the just-written slot.
        {
            uint32_t add = 0;

            idx = next_index(w->head_, w->k_);
            for (int i = 1; i < w->k_; i++)
            {
                if (w->buf_[idx] > x)
                {
                    add++;
                }
                idx++;
                if (idx == w->k_)
                {
                    idx = 0;
                }
            }

            w->inv_count_ += add;
        }

        // 4) advance head: new oldest is the next element
        w->head_++;
        if (w->head_ == w->k_)
        {
            w->head_ = 0;
        }

        if (out_count != 0)
        {
            *out_count = w->inv_count_;
        }
        return true;
    }
}

void eselproc_solve_loop(void)
{
    bool firing = false;
    inv_window_t w;
    uint8_t inv_count = 0;

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
            uint32_t i_packed_payload_0 = eselproc_lif_read_i_payload(0);
            uint32_t i_packed_payload_1 = eselproc_lif_read_i_payload(1);
            // unpack input
            uint32_t k = (i_packed_payload_0 & 0x0fu); // stay native 32-bit
            uint32_t payload_nums_last_flag =
                i_packed_payload_0 & (1 << 8); // keep as flag, can be bit-wise OR'd to last output. faster.
            int32_t nums_fragment = (int32_t)i_packed_payload_1;

            if (!firing)
            {
                firing = true; // new
                invwin_init(&w, k);
            }
            if (invwin_push(&w, nums_fragment, &inv_count))
            {
                uint32_t packed_out = payload_nums_last_flag | (uint32_t)inv_count;
                eselproc_lif_write_o_payload(0, packed_out);
                eselproc_lif_push_o_payload();
            }
            if (payload_nums_last_flag)
            {
                // pseudo-leave by resetting our firing var. Next arriving stream will have to init the window.
                firing = 0;
            }
        }
    }
}
