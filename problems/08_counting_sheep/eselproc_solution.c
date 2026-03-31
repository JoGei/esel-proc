#include "stdint.h"
#include "eselproc_drv.h"

#define LIF_FIFO_DEPTH 1

#define MAX_VALUE 0x0fffffu
#define N_MAX_DIGITS 10
#define SEEN_ALL 0x03FF

// I did not research this problem at all. This div/mod based approach is trivially out of my 10-finger=10-digit monkey
// brain. Trivial solution is to just divide/modulo by 10 in a loop and wait until you have seen all digits in the
// residual of the modulo Problem is, I try to avoid multiplies to minimize area cost of the CPU (rv32i only!) picorv32
// (rv32i) barely makes it "in time", i.e. before latchup throughput tests time out. serv does not make it.
//#define USE_MODULE_DIV
#ifndef USE_MODULO_DIV
#define USE_BASE10_SUB
// This solution avoids the multiplier by subtracting k-base10s (1, 10, 100, ...) from the candidates (N+=N) and
// counting the times it can subtract. So worst case should be all 9s because we have to run 9 rounds of each 9
// substractions on the candidate. example:  123456 |  candidate ommitted the higher bases for clarity
//           ------    digit = 0; test with base=100000
//         - 100000    ++digit
//         = 023456 |  -> rem < base finished (seen "1"); digit=0; base=010000
//         - 010000 |  ++digit
//         = 013456 |  -> rem >= base continue
//         - 010000 |  ++digit
//         = 003456 |  -> rem < base finished (seen "2"); digit=0; base=001000
//         - 001000 |  ++digit
//         = 002456 |  -> rem >= base continue
//         - 001000 |  ++digit
//         = 001456 |  -> rem >= base continue
//         - 001000 |  ++digit
//         = 000456 |  -> rem < base finished (seen "3"); digit=0; base=000100
//         - 000100 |  ++digit
//         = 000356 |  -> rem >= base continue
//         - 000100 |  ++digit
//         = 000256 |  -> rem >= base continue
//         - 000100 |  ++digit
//         = 000156 |  -> rem >= base continue
//         - 000100 |  ++digit
//         = 000056 |  -> rem < base finished (seen "4"); digit=0; base=000010
//         - 000010 |  ++digit
//         = 000046 |  -> rem >= base continue
//         - 000010 |  ++digit
//         = 000036 |  -> rem >= base continue
//         - 000010 |  ++digit
//         = 000026 |  -> rem >= base continue
//         - 000010 |  ++digit
//         = 000016 |  -> rem >= base continue
//         - 000010 |  ++digit
//         = 000006 |  -> rem < base finished (seen "5"); digit=0; base=000001
//         - 000001 |  ++digit
//         = 000005 |  -> rem >= base continue
//         - 000001 |  ++digit
//         = 000004 |  -> rem >= base continue
//         - 000001 |  ++digit
//         = 000003 |  -> rem >= base continue
//         - 000001 |  ++digit
//         = 000002 |  -> rem >= base continue
//         - 000001 |  ++digit
//         = 000001 |  -> rem >= base continue
//         - 000001 |  ++digit
//         = 000000 |  -> rem < base finished (seen "6");
//         test: seen all digits? -> no new candidate (N+=N) and start again
//         --------    try next
//         ....
static const uint32_t p10[] = { 1000000000u, 100000000u, 10000000u, 1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u };
#define POWER10(i)            \
    ((i) == 0   ? 1000000000u \
     : (i) == 1 ? 100000000u  \
     : (i) == 2 ? 10000000u   \
     : (i) == 3 ? 1000000u    \
     : (i) == 4 ? 100000u     \
     : (i) == 5 ? 10000u      \
     : (i) == 6 ? 1000u       \
     : (i) == 7 ? 100u        \
     : (i) == 8 ? 10u         \
                : 1u)
#endif

void eselproc_solve_loop(void)
{
    uint32_t seen_flag = 0;
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
            // unpack input
            uint32_t number =
                (i_packed_payload_0 & MAX_VALUE); // stay native 32-bit but filter 20 bits -> useless but more true to
                                                  // the intent of the problem at hand giving us 20 bits
            uint32_t candidate_number = 0;
            if (number != 0)
                while (seen_flag != SEEN_ALL)
                {
                    candidate_number += number;

#ifdef USE_MODULE_DIV
                    for (uint32_t quotient = candidate_number; quotient != 0; quotient = quotient / 10)
                    {
                        uint32_t residual = quotient % 10;
                        seen_flag |= (1 << residual);
                        if (seen_flag == SEEN_ALL)
                            break;
                    }
#else // is USE_BASE10_SUB
                    uint32_t value = candidate_number;
                    uint32_t started = 0u;
                    for (int i = 0; i < 10; ++i)
                    {
                        // uint32_t base = POWER10(i);
                        uint32_t base = p10[i]; // static is faster?
                        uint32_t digit = 0u;

                        while (value >= base)
                        {
                            value -= base;
                            ++digit;
                        }

                        if (digit != 0u || started)
                        {
                            started = 1;
                            seen_flag |= (1u << digit);
                        }

                        if (seen_flag == SEEN_ALL)
                            break;
                    }
#endif
                }

            eselproc_lif_write_o_payload(0, candidate_number);
            eselproc_lif_push_o_payload();
            seen_flag = 0;
        }
    }
}
