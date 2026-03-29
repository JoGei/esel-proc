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

static void swap_u8(uint8_t *a, uint8_t *b) {
    if (*a > *b) {
        uint8_t t = *a;
        *a = *b;
        *b = t;
    }
}

// solution not really optimized for streaming yet.
uint8_t median5_u8(const uint8_t x[5]) {
    uint8_t a = x[0];
    uint8_t b = x[1];
    uint8_t c = x[2]; // << values will be sorted such that c is median
    uint8_t d = x[3];
    uint8_t e = x[4];

    // partial sorting network for median of 5
    swap_u8(&a, &b);
    swap_u8(&d, &e);
    swap_u8(&a, &c);
    swap_u8(&b, &c);
    swap_u8(&a, &d);
    swap_u8(&c, &d);
    swap_u8(&b, &e);
    swap_u8(&b, &c);

    // c is now the median
    return c;
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

static void send_operands_one_cycle(VSolution *dut, uint32_t i_payload_stream_fragment, uint32_t i_payload_stream_last)
{
    dut->i_valid = 1;
    dut->i_payload_stream_fragment = i_payload_stream_fragment;
    dut->i_payload_stream_last = i_payload_stream_last;
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    //dut->i_payload_stream_fragment = 0;
    //dut->i_payload_stream_last = 0;
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

static void run_streaming_test(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1000000)
{
    auto get_expect = [](auto inp) {
        std::vector<uint8_t> exp;
        auto exp_len = inp.size() - 4;
        auto rawptr = inp.data();
        for (int i = 0; i < exp_len; ++i)
        {
            exp.push_back(median5_u8(rawptr));
            ++rawptr;
        }
        return exp;
    };

    std::vector<std::vector<uint8_t>> inputs = {
        //{ 1, 2, 3, 4, 5},
        //{ 1, 2, 3, 4, 5, 6 },
        //{ 1, 2, 3, 4, 5, 6, 7 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8, 9 },
        //{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
        { 3, 1, 4, 1, 5, 6 },
        { 1, 100, 2, 99, 3, 98, 4, 97, 5, 96},
        { 25, 178, 89, 69, 36, 255, 238, 120, 52, 22, 251, 207, 184, 65, 60, 7, 204, 32, 130, 79, 170, 80, 104, 131, 62, 86, 19, 121, 37, 146, 
          182, 127, 144, 144, 236, 43, 212, 108, 193, 60, 163, 117, 85, 66, 74, 239, 82, 88, 139, 129, 38, 4, 174, 238, 209, 4, 249, 198, 73, 
          1, 36, 117, 231, 180, 253, 101, 50, 125, 72, 120, 100, 168, 16, 218, 237, 227, 61, 86, 229, 253, 136, 137, 175, 4, 134, 98, 8, 159, 
          147, 151, 151, 254, 56, 149, 103, 240, 249, 163, 197, 9, 44, 221, 233, 77, 36, 56, 21, 153, 137, 166, 173, 185, 91, 146, 202, 3, 154,
          229, 79, 149, 107, 61, 227, 82, 228, 78, 138, 139, 74, 148, 6, 136, 90, 249, 192, 20, 37, 32, 8, 151, 106, 207, 114, 112, 160, 27, 93,
          189, 65, 143, 254, 21, 110, 96, 38, 194, 67, 8, 71, 216, 7, 145, 36, 193, 25, 212, 138, 245, 41, 17, 135, 231, 231, 146, 119, 4, 142, 
          76, 66, 189, 186, 139, 49, 161, 165, 73, 47, 138, 186, 125, 124 }
        
    };

    for(auto& input: inputs)
    {

        std::deque<uint8_t> expected1_queue;
        //for(auto const & exp: get_expect(input)) {
        //    expected1_queue.push_back(exp);
        //}
        std::cout << "input.size()=" << input.size() << std::endl;
        auto exps =  get_expect(input);
        size_t send_idx = 0;
        size_t recv_idx = 0;

        clear_input(dut);
        drain_stale_output(dut, tfp);

        bool prev_o_valid = dut->o_valid;
        int cycle;
        for (cycle = 0; cycle < max_cycles; ++cycle)
        {
            if (send_idx < input.size() && dut->i_ready)
            {
                uint8_t i_payload_stream_fragment = input[send_idx];
                uint8_t i_payload_stream_last = send_idx == (input.size() -1);

                if(send_idx >= 4)
                {
                    expected1_queue.push_back(exps[send_idx-4]);
                }                
                send_operands_one_cycle(dut, i_payload_stream_fragment, i_payload_stream_last);
    
                std::cout << "[stream] send  i_payload_stream_fragment=" << (int)i_payload_stream_fragment 
                          << " i_payload_stream_last=" << (int)i_payload_stream_last << " send_idx=" << send_idx;
                std::cout << " exp?=" << (send_idx >= 4 ? std::to_string((expected1_queue.back())) : " -") << std::endl;

                send_idx++;
            }
            else
            {
                clear_input(dut);
            }

            tick(dut, tfp);

            bool cur_o_valid = dut->o_valid;
            if (cur_o_valid //&& !prev_o_valid 
                && send_idx > 4)
            {
                if (expected1_queue.empty())
                {
                    std::stringstream ss;
                    ss << "ERROR: got unexpected output1 ..\n"
                    << dut->o_payload_fragment << "\n"
                    << "" << dut->o_payload_last << "\n";
                    fail(dut, tfp, ss.str().c_str());
                }

                uint8_t expected1 = expected1_queue.front();
                expected1_queue.pop_front();
                uint8_t expected2 = (recv_idx+1  == input.size()-4);

                uint8_t got1 = dut->o_payload_fragment;
                uint8_t got2 = dut->o_payload_last;

                std::cout << "[stream] recv["<< recv_idx<< "]:\n" 
                          << "  got1=" << (int)got1 << " exp1=" << (int)expected1 << std::endl
                          << "  got2=" << (int)got2 << " exp2=" << (int)expected2 << std::endl;

                if (got1 != expected1 || got2 != expected2)
                {
                    std::stringstream ss;
                    ss << "ERROR: streaming mismatch at result ..\n"
                       << recv_idx << ": got1=" << (int)got1 << " exp1=" << (int)expected1 << std::endl
                       << recv_idx << ": got2=" << (int)got2 << " exp2=" << (int)expected2 << std::endl;
                    fail(dut, tfp, ss.str().c_str());
                }

                recv_idx++;
            }
            prev_o_valid = cur_o_valid;
            if (send_idx == input.size() && recv_idx == input.size()-4)
                break;
        }
        std::cout << "send_idx: " << send_idx << " recv_idx: " << recv_idx << std::endl;
        std::cout << "exp_send_idx: " << input.size() << " exp_recv_idx: " << input.size()-4 << std::endl;
        if (send_idx == input.size() && recv_idx == input.size()-4)
        {
            clear_input(dut);
            std::cout << "Streaming test passed.\n";
            std::cout << "finished stream after [" << cycle << "] clock cycles." << std::endl;
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
    dut->i_payload_stream_fragment = 0;
    dut->i_payload_stream_last = 0;

    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);

    dut->reset = 0;
    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);
}

static void send_operands(VSolution *dut, std::vector<uint8_t> a, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "> [send operands] started." << std::endl;

    for(int i = 0; i < a.size(); ++i)
    {
        
        wait_ready(dut, tfp, 10000);

        dut->i_valid = 1;
        dut->i_payload_stream_fragment = a[i];
        dut->i_payload_stream_last = i == (a.size() - 1);

        // One cycle is enough: i_ready was already high.
        tick(dut, tfp);

        dut->i_valid = 0;
        dut->i_payload_stream_fragment = 0;
        dut->i_payload_stream_last = 0;
        
    }

    std::cout << "> [send operands] completed." << std::endl;
}

static bool recv_result(VSolution *dut, VerilatedVcdC *tfp, uint32_t &got1, int max_cycles = 100000)
{
    std::cout << "> [recv operands] started." << std::endl;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);
        if (dut->o_valid)
        {
            std::cout << "> [recv operands] completed." << std::endl;
            got1 = dut->o_payload_fragment;
            return dut->o_payload_last;
        }
    }

    std::stringstream ss;
    ss << "ERROR: timeout waiting for o_valid after " << max_cycles << " cycles\n";

    fail(dut, tfp, ss.str().c_str());
    got1 = 0xAAAAAAAA;
    return 1; // never reached
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
static void run_case(VSolution *dut, std::vector<uint8_t> a, VerilatedVcdC *tfp = nullptr)
{
    const uint32_t expected1 = median5_u8(a.data());

    std::cout << "Test: a={";
    for (const auto& it: a) std::cout << " " << (int)it << " ";
    std::cout << "}" << std::endl;

    send_operands(dut, a, tfp);
    uint32_t got1 = -1;
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
    tfp->open("tb_median_filter.vcd");

    reset_dut(dut, tfp);
 
    run_case(dut, { 01, 02, 03, 04, 05 }, tfp);
    run_case(dut, { 11, 22, 33, 44, 55 }, tfp);
    run_case(dut, { 15, 24, 33, 42, 51 }, tfp);
    run_case(dut, { 00, 00, 00, 00, 00 }, tfp);
    run_case(dut, { 0xff, 0xff, 0xff, 0xff, 0xff }, tfp);
    run_case(dut, { 01, 01, 01, 01, 01 }, tfp);

    std::cout << "All single requests tests passed." << std::endl;

    //run_streaming_sequential_test(dut, tfp);

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}