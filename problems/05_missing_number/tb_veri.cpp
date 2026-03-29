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

// solution not really optimized for streaming yet.
auto get_expect = [](const auto& inp) {
    int m = 0;
    int cnt = 0;
    for (const auto& n: inp)
    {
        m ^= n ^ cnt;
        ++cnt;
    }
    return m ^ cnt;
};

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

static void send_operands_one_cycle(VSolution *dut, uint32_t i_payload_nums_fragment, uint32_t i_payload_nums_last)
{
    dut->i_valid = 1;
    dut->i_payload_nums_fragment = i_payload_nums_fragment;
    dut->i_payload_nums_last = i_payload_nums_last;
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    dut->i_payload_nums_fragment = 0;
    dut->i_payload_nums_last = 0;
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
    std::vector<std::vector<uint8_t>> inputs = {
        //{ 1, 2, 3, 4, 5},
        //{ 1, 2, 3, 4, 5, 6 },
        //{ 1, 2, 3, 4, 5, 6, 7 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8, 9 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
        {    2, 3, 4, 5 },
        { 1,    3, 4, 5 },
        { 1, 2,    4, 5 },
        { 1, 2, 3,    5 },
        { 1, 2, 3, 4    }
    };

    for(auto& input: inputs)
    {

        std::deque<uint8_t> expected1_queue;
        //for(auto const & exp: get_expect(input)) {
        //    expected1_queue.push_back(exp);
        //}
        std::cout << "input.size()=" << input.size() << std::endl;
        size_t send_idx = 0;
        size_t recv_idx = 0;

        clear_input(dut);
        drain_stale_output(dut, tfp);

        bool prev_o_valid = dut->o_valid;

        for (int cycle = 0; cycle < max_cycles; ++cycle)
        {
            if (send_idx < input.size() && dut->i_ready)
            {
                uint8_t i_payload_nums_fragment = input[send_idx];
                uint8_t i_payload_nums_last = send_idx == (input.size() -1);

                if(i_payload_nums_last)
                {
                    expected1_queue.push_back(get_expect(input));
                }                
                send_operands_one_cycle(dut, i_payload_nums_fragment, i_payload_nums_last);
    
                std::cout << "[stream] send  i_payload_nums_fragment=" << (int)i_payload_nums_fragment 
                          << " i_payload_nums_last=" << (int)i_payload_nums_last << " send_idx=" << send_idx;
                std::cout << " exp?=" << (i_payload_nums_last ? std::to_string((expected1_queue.back())) : " -") << std::endl;

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
                if (expected1_queue.empty())
                {
                    std::stringstream ss;
                    ss << "ERROR: got unexpected output1 ..\n"
                    << dut->o_payload << "\n";
                    fail(dut, tfp, ss.str().c_str());
                }

                uint8_t expected1 = expected1_queue.front();
                expected1_queue.pop_front();

                uint8_t got1 = dut->o_payload;

                std::cout << "[stream] recv["<< recv_idx<< "]:\n" 
                          << "  got1=" << (int)got1 << " exp1=" << (int)expected1 << std::endl;

                if (got1 != expected1)
                {
                    std::stringstream ss;
                    ss << "ERROR: streaming mismatch at result ..\n"
                       << recv_idx << ": got1=" << (int)got1 << " exp1=" << (int)expected1 << std::endl;
                    fail(dut, tfp, ss.str().c_str());
                }

                recv_idx++;
            }
            prev_o_valid = cur_o_valid;
            if (send_idx == input.size() && recv_idx == 1)
                break;
        }
        std::cout << "send_idx: " << send_idx << " recv_idx: " << recv_idx << std::endl;
        std::cout << "exp_send_idx: " << input.size() << " exp_recv_idx: " << 1 << std::endl;
        if (send_idx == input.size() && recv_idx == 1)
        {
            clear_input(dut);
            std::cout << "Streaming test passed.\n";
        }
        else
        {
            std::stringstream ss;
            ss << "ERROR: streaming test timeout. sent=" << send_idx << " received=" << recv_idx
            << " pending1=" << expected1_queue.size() << "\n";

            fail(dut, tfp, ss.str().c_str());
        }
    }
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
    dut->i_payload_nums_fragment = 0;
    dut->i_payload_nums_last = 0;

    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);

    dut->reset = 0;
    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);
}

static void send_operands(VSolution *dut, std::vector<uint16_t> a, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "> [send operands] started." << std::endl;

    for(int i = 0; i < a.size(); ++i)
    {
        
        wait_ready(dut, tfp, 10000);

        dut->i_valid = 1;
        dut->i_payload_nums_fragment = a[i];
        dut->i_payload_nums_last = i == (a.size() - 1);

        // One cycle is enough: i_ready was already high.
        tick(dut, tfp);

        dut->i_valid = 0;
        dut->i_payload_nums_fragment = 0;
        dut->i_payload_nums_last = 0;
        
    }

    std::cout << "> [send operands] completed." << std::endl;
}

static bool recv_result(VSolution *dut, VerilatedVcdC *tfp, uint16_t &got1, int max_cycles = 100000)
{
    std::cout << "> [recv operands] started." << std::endl;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);
        if (dut->o_valid)
        {
            std::cout << "> [recv operands] completed." << std::endl;
            got1 = dut->o_payload;
            return true;
        }
    }

    std::stringstream ss;
    ss << "ERROR: timeout waiting for o_valid after " << max_cycles << " cycles\n";

    fail(dut, tfp, ss.str().c_str());
    got1 = 0xAAAA;
    return false; // never reached
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
static void run_case(VSolution *dut, std::vector<uint16_t> a, VerilatedVcdC *tfp = nullptr)
{
    const uint16_t expected1 = get_expect(a);

    std::cout << "Test: a={";
    for (const auto& it: a) std::cout << " " << (int)it << " ";
    std::cout << "}" << std::endl;

    send_operands(dut, a, tfp);
    uint16_t got1 = -1;
    bool last_asserted = recv_result(dut, tfp, got1);
    std::cout << "  expected = " << expected1 << std::endl;
    std::cout << "  got      = " << got1 << std::endl;

    wait_o_valid_clear(dut, tfp);
    if (got1 != expected1 || last_asserted == 0)
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
    tfp->open("tb_missing_number.vcd");

    reset_dut(dut, tfp);
 
    run_case(dut, {     02, 03, 04, 05 }, tfp);
    run_case(dut, { 01,     03, 04, 05 }, tfp);
    run_case(dut, { 01, 02,     04, 05 }, tfp);
    run_case(dut, { 01, 02, 03,     05 }, tfp);
    run_case(dut, { 01, 02, 03, 04,     06 }, tfp);

    std::cout << "All single requests tests passed." << std::endl;

    //run_streaming_sequential_test(dut, tfp);

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}