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

static void send_operands_one_cycle(VSolution *dut, uint32_t a, uint32_t b)
{
    dut->i_valid = 1;
    dut->i_payload_dividend = a;
    dut->i_payload_divisor = b;
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    dut->i_payload_dividend = 0;
    dut->i_payload_divisor = 0;
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

static void run_streaming_test(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 500000)
{
    std::vector<std::pair<uint32_t, uint32_t>> inputs = {
        { 3, 7 }, { 12, 13 }, { 255, 255 }, { 1024, 17 }, { 32767, 2 }, { 65535, 1 }, { 100, 200 }, { 17, 19 },
    };

    std::deque<uint32_t> expected1_queue;
    std::deque<uint32_t> expected2_queue;
    size_t send_idx = 0;
    size_t recv_idx = 0;

    clear_input(dut);
    drain_stale_output(dut, tfp);

    bool prev_o_valid = dut->o_valid;

    for (int cycle = 0; cycle < max_cycles; ++cycle)
    {
        if (send_idx < inputs.size() && dut->i_ready)
        {
            uint32_t a = inputs[send_idx].first;
            uint32_t b = inputs[send_idx].second;
            uint32_t exp1 = b ? static_cast<uint32_t>(a) / static_cast<uint32_t>(b) : ~0;
            uint32_t exp2 = b ? static_cast<uint32_t>(a) % static_cast<uint32_t>(b) : ~0;

            send_operands_one_cycle(dut, a, b);
            expected1_queue.push_back(exp1);
            expected2_queue.push_back(exp2);

            std::cout << "[stream] send  a=" << a << " b=" << b << " exp1=" << exp1 << " exp2=" << exp2 << "\n";

            send_idx++;
        }
        else
        {
            clear_input(dut);
        }

        tick(dut, tfp);

        bool cur_o_valid = dut->o_valid;
        if (cur_o_valid && !prev_o_valid && send_idx > 0)
        {
            if (expected1_queue.empty() || expected2_queue.empty())
            {
                std::stringstream ss;
                ss << "ERROR: got unexpected output1 ..\n"
                   << dut->o_payload_1 << "\n"
                   << "" << dut->o_payload_2 << "\n";
                fail(dut, tfp, ss.str().c_str());
            }

            uint32_t expected1 = expected1_queue.front();
            uint32_t expected2 = expected2_queue.front();
            expected1_queue.pop_front();
            expected2_queue.pop_front();

            uint32_t got1 = dut->o_payload_1;
            uint32_t got2 = dut->o_payload_2;

            std::cout << "[stream] recv  got1=" << got1 << " exp1=" << expected1 << "\n";
            std::cout << "               got2=" << got2 << " exp2=" << expected2 << "\n";

            if (got1 != expected1 || got2 != expected2)
            {
                std::stringstream ss;
                ss << "ERROR: streaming mismatch at result ..\n"
                   << recv_idx << ": got1=" << got1 << " expected1=" << expected1
                   << "                                        " << recv_idx << ": got2=" << got2
                   << " expected2=" << expected2 << "\n";
                fail(dut, tfp, ss.str().c_str());
            }

            recv_idx++;
        }
        prev_o_valid = cur_o_valid;

        if (send_idx == inputs.size() && recv_idx == inputs.size())
        {
            clear_input(dut);
            std::cout << "Streaming test passed.\n";
            return;
        }
    }

    std::stringstream ss;
    ss << "ERROR: streaming test timeout. sent=" << send_idx << " received=" << recv_idx
       << " pending1=" << expected1_queue.size() << "\n"
       << " pending2=" << expected2_queue.size() << "\n";

    fail(dut, tfp, ss.str().c_str());
}

static void run_streaming_sequential_test(VSolution *dut, VerilatedVcdC *tfp = nullptr)
{
    std::vector<std::pair<uint32_t, uint32_t>> inputs = {
        { 3, 7 },     { 12, 13 },   { 255, 255 }, { 1024, 17 },     { 32767, 2 },
        { 65535, 1 }, { 100, 200 }, { 17, 19 },   { 65535, 65535 },
    };

    for (size_t idx = 0; idx < inputs.size(); ++idx)
    {
        uint32_t a = inputs[idx].first;
        uint32_t b = inputs[idx].second;

        uint32_t expected1 = b ? static_cast<uint32_t>(a) / static_cast<uint32_t>(b) : ~0;
        uint32_t expected2 = b ? static_cast<uint32_t>(a) % static_cast<uint32_t>(b) : ~0;

        std::cout << "[stream-seq] case " << idx << " a=" << a << " b=" << b << "\n";

        // Wait until DUT is ready to accept a new input
        int ready_timeout = 1000;
        while (!dut->i_ready && ready_timeout-- > 0)
        {
            tick(dut, tfp);
        }
        if (!dut->i_ready)
        {

            fail(dut, tfp, "ERROR: timeout waiting for i_ready before send\n");
        }

        // Send exactly one request
        dut->i_valid = 1;
        dut->i_payload_dividend = a;
        dut->i_payload_divisor = b;
        tick(dut, tfp);
        dut->i_valid = 0;
        dut->i_payload_dividend = 0;
        dut->i_payload_divisor = 0;

        // Wait for result
        int valid_timeout = 100000;
        while (!dut->o_valid && valid_timeout-- > 0)
        {
            tick(dut, tfp);
        }
        if (!dut->o_valid)
        {
            fail(dut, tfp, "ERROR: timeout waiting for o_valid\n");
        }

        uint32_t got1 = dut->o_payload_1;
        uint32_t got2 = dut->o_payload_2;

        std::cout << "  expected1 = " << expected1 << "\n";
        std::cout << "  got1      = " << got1 << "\n";
        std::cout << "  expected2 = " << expected2 << "\n";
        std::cout << "  got2      = " << got2 << "\n";

        if (got1 != expected1 || got2 != expected2)
        {
            std::stringstream ss;
            ss << "ERROR: mismatch in streaming sequential test at case " << idx << "\n";
            fail(dut, tfp, ss.str().c_str());
        }

        // Wait for o_valid to drop before starting next transaction,
        // so we don't accidentally re-consume the old result.
        int clear_timeout = 1000;
        while (dut->o_valid && clear_timeout-- > 0)
        {
            tick(dut, tfp);
        }
        if (dut->o_valid)
        {
            fail(dut, tfp, "ERROR: o_valid stayed high too long after result consumption\n");
        }
    }

    std::cout << "Sequential streaming test passed.\n";
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
    dut->i_payload_dividend = 0;
    dut->i_payload_divisor = 0;

    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);

    dut->reset = 0;
    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);
}

static void send_operands(VSolution *dut, uint32_t a, uint32_t b, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "> [send operands] started." << std::endl;

    wait_ready(dut, tfp, 10000);

    dut->i_valid = 1;
    dut->i_payload_dividend = a;
    dut->i_payload_divisor = b;

    // One cycle is enough: i_ready was already high.
    tick(dut, tfp);

    dut->i_valid = 0;
    dut->i_payload_dividend = 0;
    dut->i_payload_divisor = 0;

    std::cout << "> [send operands] completed." << std::endl;
}

static void recv_result(VSolution *dut, VerilatedVcdC *tfp, uint32_t &got1, uint32_t &got2, int max_cycles = 100000)
{
    std::cout << "> [recv operands] started." << std::endl;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);
        if (dut->o_valid)
        {
            std::cout << "> [recv operands] completed." << std::endl;
            got1 = dut->o_payload_1;
            got2 = dut->o_payload_2;
            return;
        }
    }

    std::stringstream ss;
    ss << "ERROR: timeout waiting for o_valid after " << max_cycles << " cycles\n";

    fail(dut, tfp, ss.str().c_str());
    got1 = 0xAAAAAAAA;
    got2 = 0x55555555;
    return; // never reached
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
static void run_case(VSolution *dut, uint32_t a, uint32_t b, VerilatedVcdC *tfp = nullptr)
{
    const uint32_t expected1 = b ? static_cast<uint32_t>(a) / static_cast<uint32_t>(b) : ~0;
    const uint32_t expected2 = b ? static_cast<uint32_t>(a) % static_cast<uint32_t>(b) : ~0;

    std::cout << "Test: a=" << a << ", b=" << b << std::endl;

    send_operands(dut, a, b, tfp);
    uint32_t got1 = -1, got2 = -1;
    recv_result(dut, tfp, got1, got2);
    std::cout << "  expected1 = " << expected1 << std::endl;
    std::cout << "  got1      = " << got1 << std::endl;
    std::cout << "  expected2 = " << expected2 << std::endl;
    std::cout << "  got2      = " << got2 << std::endl;

    wait_o_valid_clear(dut, tfp);
    if (got1 != expected1 || (got2 != expected2))
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
    tfp->open("tb_divider.vcd");

    reset_dut(dut, tfp);

    run_case(dut, 0, 0, tfp);
    run_case(dut, 1, 1, tfp);
    run_case(dut, 3, 7, tfp);
    run_case(dut, 12, 13, tfp);
    run_case(dut, 255, 255, tfp);
    run_case(dut, 1024, 17, tfp);
    run_case(dut, 32767, 2, tfp);
    run_case(dut, 65535, 1, tfp);
    run_case(dut, 65535, 2, tfp);
    run_case(dut, 65535, 65535, tfp);

    std::cout << "All single requests tests passed." << std::endl;

    run_streaming_sequential_test(dut, tfp);

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}