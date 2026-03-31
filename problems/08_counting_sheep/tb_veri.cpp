#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VSolution.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <deque>
#include <utility>
#include <sstream>

static constexpr uint32_t SEEN_ALL = 0x03FFu;
static constexpr uint32_t INSOMNIA_VALUE = 0u;
static constexpr uint32_t MAX_INPUT_MASK = 0x0FFFFFu;

// Reference model for Counting Sheep.
// This is only for the testbench, so normal /10 and %10 are fine here.
static uint32_t counting_sheep_ref(uint32_t n)
{
    n &= MAX_INPUT_MASK;

    if (n == 0u)
        return INSOMNIA_VALUE;

    uint32_t seen = 0u;
    uint32_t cur = 0u;

    while (seen != SEEN_ALL)
    {
        cur += n;

        uint32_t x = cur;
        while (x != 0u)
        {
            uint32_t d = x % 10u;
            seen |= (1u << d);
            x /= 10u;
        }
    }

    return cur;
}

static vluint64_t main_time = 0;
double sc_time_stamp()
{
    return (double)main_time;
}

static void fail(VSolution *dut, VerilatedVcdC *tfp, const char *msg)
{
    if (dut)
        dut->eval();
    if (tfp)
    {
        tfp->dump(main_time);
        tfp->flush();
        tfp->close();
    }
    std::cerr << "ERROR: " << msg << "\n";
    std::exit(1);
}

static void tick(VSolution *dut, VerilatedVcdC *tfp = nullptr)
{
    dut->clk = 0;
    dut->eval();
    if (tfp)
        tfp->dump(main_time);
    main_time++;

    dut->clk = 1;
    dut->eval();
    if (tfp)
        tfp->dump(main_time);
    main_time++;
}

static void send_operands_one_cycle(VSolution *dut, uint32_t x)
{
    dut->i_valid = 1;
    dut->i_payload_N = (x & MAX_INPUT_MASK);
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    dut->i_payload_N = 0;
}

static void drain_stale_output(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 200)
{
    for (int i = 0; i < max_cycles; ++i)
    {
        if (!dut->o_valid)
            return;
        tick(dut, tfp);
    }

    fail(dut, tfp, "ERROR: stale o_valid did not clear before streaming test");
}

static void run_streaming_test(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 3000000)
{
    std::vector<uint32_t> inputs = {
        0u,
        1u,
        2u,
        3u,
        7u,
        10u,
        11u,
        12u,
        19u,
        1692u,
        999u,
        12345u,
        54321u,
        99999u,
        0x0FFFFFu,
        999999 // worst case?
    };

    std::deque<uint32_t> expected_queue;
    size_t send_idx = 0;
    size_t recv_idx = 0;

    clear_input(dut);
    drain_stale_output(dut, tfp);

    bool prev_o_valid = dut->o_valid;

    for (int cycle = 0; cycle < max_cycles; ++cycle)
    {
        if (send_idx < inputs.size() && dut->i_ready)
        {
            uint32_t x = inputs[send_idx];
            uint32_t exp = counting_sheep_ref(x);

            send_operands_one_cycle(dut, x);
            expected_queue.push_back(exp);

            std::cout << "[stream] send  n=" << (x & MAX_INPUT_MASK)
                      << " exp=" << exp << "\n";

            send_idx++;
        }
        else
        {
            clear_input(dut);
        }

        tick(dut, tfp);

        bool cur_o_valid = dut->o_valid;
        if (cur_o_valid && !prev_o_valid)
        {
            if (expected_queue.empty())
            {
                std::stringstream ss;
                ss << "ERROR: got unexpected output " << dut->o_payload << "\n";
                fail(dut, tfp, ss.str().c_str());
            }

            uint32_t expected = expected_queue.front();
            expected_queue.pop_front();

            uint32_t got = dut->o_payload;

            std::cout << "[stream] recv  got=" << got
                      << " exp=" << expected << "\n";

            if (got != expected)
            {
                std::stringstream ss;
                ss << "ERROR: streaming mismatch at result " << recv_idx
                   << ": got=" << got
                   << " expected=" << expected << "\n";
                fail(dut, tfp, ss.str().c_str());
            }

            recv_idx++;
        }
        prev_o_valid = cur_o_valid;

        if (send_idx == inputs.size() && recv_idx == inputs.size())
        {
            clear_input(dut);
            std::cout << "Streaming test passed.\n";
            std::cout << "finished stream after [" << cycle << "] clock cycles." << std::endl;
            return;
        }
    }

    std::stringstream ss;
    ss << "ERROR: streaming test timeout. sent=" << send_idx
       << " received=" << recv_idx
       << " pending=" << expected_queue.size() << "\n";

    fail(dut, tfp, ss.str().c_str());
}

static void wait_ready(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1000)
{
    for (int i = 0; i < max_cycles; ++i)
    {
        if (dut->i_ready)
            return;
        tick(dut, tfp);
    }

    std::stringstream ss;
    ss << "ERROR: timeout waiting for i_ready after " << max_cycles << " cycles\n";
    fail(dut, tfp, ss.str().c_str());
}

static void reset_dut(VSolution *dut, VerilatedVcdC *tfp = nullptr)
{
    dut->reset = 1;
    dut->i_valid = 0;
    dut->i_payload_N = 0;

    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);

    dut->reset = 0;
    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);
}

static void send_operands(VSolution *dut, uint32_t x, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "> [send operands] started." << std::endl;

    wait_ready(dut, tfp, 10000);

    dut->i_valid = 1;
    dut->i_payload_N = (x & MAX_INPUT_MASK);

    tick(dut, tfp);

    dut->i_valid = 0;
    dut->i_payload_N = 0;

    std::cout << "> [send operands] completed." << std::endl;
}

static uint32_t recv_result(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1000000)
{
    std::cout << "> [recv result] started." << std::endl;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);
        if (dut->o_valid)
        {
            std::cout << "> [recv result] completed." << std::endl;
            return dut->o_payload;
        }
    }

    std::stringstream ss;
    ss << "ERROR: timeout waiting for o_valid after " << max_cycles << " cycles\n";
    fail(dut, tfp, ss.str().c_str());
    return 0;
}

static void wait_o_valid_clear(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1000)
{
    for (int i = 0; i < max_cycles; ++i)
    {
        if (!dut->o_valid)
            return;
        tick(dut, tfp);
    }

    std::stringstream ss;
    ss << "ERROR: o_valid stayed high too long\n";
    fail(dut, tfp, ss.str().c_str());
}

static void run_case(VSolution *dut, uint32_t x, VerilatedVcdC *tfp = nullptr)
{
    const uint32_t masked_x = x & MAX_INPUT_MASK;
    const uint32_t expected = counting_sheep_ref(masked_x);

    std::cout << "Test: n=" << masked_x << std::endl;

    send_operands(dut, masked_x, tfp);
    uint32_t got = recv_result(dut, tfp);

    std::cout << "  expected = " << expected << std::endl;
    std::cout << "  got      = " << got << std::endl;

    wait_o_valid_clear(dut, tfp);

    if (got != expected)
    {
        std::stringstream ss;
        ss << "FAIL: mismatch\n";
        fail(dut, tfp, ss.str().c_str());
    }
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    VSolution *dut = new VSolution;
    VerilatedVcdC *tfp = new VerilatedVcdC;

    dut->trace(tfp, 1);
    tfp->open("tb_counting_sheep.vcd");

    reset_dut(dut, tfp);

    run_case(dut, 0u, tfp);        // INSOMNIA -> 0
    run_case(dut, 1u, tfp);        // 10
    run_case(dut, 2u, tfp);
    run_case(dut, 3u, tfp);
    run_case(dut, 7u, tfp);
    run_case(dut, 10u, tfp);
    run_case(dut, 11u, tfp);
    run_case(dut, 12u, tfp);
    run_case(dut, 19u, tfp);
    run_case(dut, 1692u, tfp);     // expected 5076
    run_case(dut, 999u, tfp);
    run_case(dut, 12345u, tfp);
    run_case(dut, 54321u, tfp);
    run_case(dut, 99999u, tfp);
    run_case(dut, 0x0FFFFFu, tfp); // max 20-bit input
    run_case(dut, 999999, tfp); // worst case?

    std::cout << "All single requests tests passed." << std::endl;

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}
