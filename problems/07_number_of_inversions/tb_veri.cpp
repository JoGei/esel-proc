#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VSolution.h"

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <vector>

struct StreamCase
{
    uint8_t k;
    std::vector<int32_t> nums;
};

struct Result
{
    uint8_t count;
    bool last;
};

static uint8_t inversion_count_ref(const std::vector<int32_t> &nums, size_t start, uint8_t k)
{
    uint32_t inv = 0;
    for (size_t i = start; i < start + k; ++i)
    {
        for (size_t j = i + 1; j < start + k; ++j)
        {
            if (nums[i] > nums[j])
                inv++;
        }
    }
    return static_cast<uint8_t>(inv);
}

static std::vector<Result> number_of_inversions_ref(const StreamCase &tc)
{
    std::vector<Result> out;

    if (tc.k == 0)
        return out;

    for (size_t end = 0; end < tc.nums.size(); ++end)
    {
        if (end + 1 >= tc.k)
        {
            size_t start = end + 1 - tc.k;
            out.push_back({ inversion_count_ref(tc.nums, start, tc.k), end == (tc.nums.size() - 1) });
        }
    }

    return out;
}

static vluint64_t main_time = 0;
double sc_time_stamp()
{
    return static_cast<double>(main_time);
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

static void send_operands_one_cycle(VSolution *dut, uint8_t k, int32_t value, bool last)
{
    dut->i_valid = 1;
    dut->i_payload_k = k & 0x0Fu;
    dut->i_payload_nums_fragment = static_cast<uint32_t>(value);
    dut->i_payload_nums_last = last ? 1 : 0;
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    dut->i_payload_k = 0;
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

    fail(dut, tfp, "stale o_valid did not clear before test");
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
    ss << "timeout waiting for i_ready after " << max_cycles << " cycles";
    fail(dut, tfp, ss.str().c_str());
}

static void reset_dut(VSolution *dut, VerilatedVcdC *tfp = nullptr)
{
    dut->reset = 1;
    clear_input(dut);

    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);

    dut->reset = 0;
    for (int i = 0; i < 8; ++i)
        tick(dut, tfp);
}

static void wait_o_valid_clear(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1000)
{
    for (int i = 0; i < max_cycles; ++i)
    {
        if (!dut->o_valid)
            return;
        tick(dut, tfp);
    }

    fail(dut, tfp, "o_valid stayed high too long");
}

static void ensure_no_output(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 20000)
{
    bool prev_o_valid = dut->o_valid;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);
        bool cur_o_valid = dut->o_valid;
        if (cur_o_valid && !prev_o_valid)
            fail(dut, tfp, "unexpected output for case that should produce no results");
        prev_o_valid = cur_o_valid;
    }
}

static std::string result_to_string(const Result &res)
{
    std::stringstream ss;
    ss << "(count=" << static_cast<int>(res.count) << ", last=" << res.last << ")";
    return ss.str();
}

static void run_case(VSolution *dut, const StreamCase &tc, VerilatedVcdC *tfp = nullptr)
{
    const std::vector<Result> expected = number_of_inversions_ref(tc);

    std::cout << "Test: k=" << static_cast<int>(tc.k) << " nums={";
    for (size_t i = 0; i < tc.nums.size(); ++i)
    {
        if (i)
            std::cout << ", ";
        std::cout << tc.nums[i];
    }
    std::cout << "}" << std::endl;

    std::cout << "> [send operands] started." << std::endl;
    std::cout << "> [recv result] started." << std::endl;

    clear_input(dut);
    drain_stale_output(dut, tfp);

    size_t send_idx = 0;
    size_t recv_idx = 0;
    bool prev_o_valid = dut->o_valid;

    for (int cycle = 0; cycle < 1000000; ++cycle)
    {
        if (send_idx < tc.nums.size() && dut->i_ready)
        {
            send_operands_one_cycle(dut, tc.k, tc.nums[send_idx], send_idx == (tc.nums.size() - 1));
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
            if (recv_idx >= expected.size())
                fail(dut, tfp, "unexpected extra output");

            Result got = { static_cast<uint8_t>(dut->o_payload_fragment), dut->o_payload_last != 0 };

            std::cout << "  result[" << recv_idx << "] got=" << result_to_string(got)
                      << " exp=" << result_to_string(expected[recv_idx]) << std::endl;

            if (got.count != expected[recv_idx].count || got.last != expected[recv_idx].last)
            {
                std::stringstream ss;
                ss << "result mismatch at index " << recv_idx << ": got=" << result_to_string(got)
                   << " expected=" << result_to_string(expected[recv_idx]);
                fail(dut, tfp, ss.str().c_str());
            }

            recv_idx++;
        }

        prev_o_valid = cur_o_valid;

        if (send_idx == tc.nums.size() && recv_idx == expected.size())
        {
            clear_input(dut);
            std::cout << "> [send operands] completed." << std::endl;
            std::cout << "> [recv result] completed." << std::endl;
            wait_o_valid_clear(dut, tfp);

            if (expected.empty())
            {
                ensure_no_output(dut, tfp);
                std::cout << "  expected no outputs" << std::endl;
            }
            return;
        }
    }

    std::stringstream ss;
    ss << "timeout waiting for case completion. sent=" << send_idx << " received=" << recv_idx
       << " expected=" << expected.size();
    fail(dut, tfp, ss.str().c_str());
}

static void run_streaming_test(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 2000000)
{
    std::vector<StreamCase> inputs = {
        { 1, { 5 } },       { 3, { 3, 1, 2, 5, 4 } }, { 4, { 4, 3, 2, 1 } },      { 2, { -1, 10, -5, 7 } },
        { 5, { 1, 2, 3 } }, { 3, { 7, 7, 7, 7 } },    { 5, { 5, 1, 4, 2, 3, 0 } }
    };

    std::deque<Result> expected_queue;
    size_t total_expected = 0;
    for (const auto &tc : inputs)
    {
        std::vector<Result> expected = number_of_inversions_ref(tc);
        total_expected += expected.size();
        for (const auto &res : expected)
            expected_queue.push_back(res);
    }

    clear_input(dut);
    drain_stale_output(dut, tfp);

    size_t send_case_idx = 0;
    size_t send_elem_idx = 0;
    size_t recv_idx = 0;
    bool prev_o_valid = dut->o_valid;

    for (int cycle = 0; cycle < max_cycles; ++cycle)
    {
        if (send_case_idx < inputs.size() && dut->i_ready)
        {
            const StreamCase &tc = inputs[send_case_idx];
            bool last = send_elem_idx == (tc.nums.size() - 1);

            send_operands_one_cycle(dut, tc.k, tc.nums[send_elem_idx], last);
            std::cout << "[stream] send case=" << send_case_idx << " idx=" << send_elem_idx
                      << " k=" << static_cast<int>(tc.k) << " val=" << tc.nums[send_elem_idx] << " last=" << last
                      << std::endl;

            send_elem_idx++;
            if (send_elem_idx == tc.nums.size())
            {
                send_case_idx++;
                send_elem_idx = 0;
            }
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
                ss << "unexpected output result at recv_idx=" << recv_idx;
                fail(dut, tfp, ss.str().c_str());
            }

            Result got = { static_cast<uint8_t>(dut->o_payload_fragment), dut->o_payload_last != 0 };
            Result expected = expected_queue.front();
            expected_queue.pop_front();

            std::cout << "[stream] recv result[" << recv_idx << "] got=" << result_to_string(got)
                      << " exp=" << result_to_string(expected) << std::endl;

            if (got.count != expected.count || got.last != expected.last)
            {
                std::stringstream ss;
                ss << "streaming mismatch at result " << recv_idx << ": got=" << result_to_string(got)
                   << " expected=" << result_to_string(expected);
                fail(dut, tfp, ss.str().c_str());
            }

            recv_idx++;
        }

        prev_o_valid = cur_o_valid;

        if (send_case_idx == inputs.size() && expected_queue.empty() && recv_idx == total_expected)
        {
            clear_input(dut);
            std::cout << "Streaming test passed." << std::endl;
            std::cout << "finished stream after [" << cycle << "] clock cycles." << std::endl;
            return;
        }
    }

    std::stringstream ss;
    ss << "streaming test timeout. sent_cases=" << send_case_idx << " received=" << recv_idx
       << " pending=" << expected_queue.size();
    fail(dut, tfp, ss.str().c_str());
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    VSolution *dut = new VSolution;
    VerilatedVcdC *tfp = new VerilatedVcdC;

    dut->trace(tfp, 1);
    tfp->open("tb_number_of_inversions.vcd");

    reset_dut(dut, tfp);

    run_case(dut, { 1, { 5 } }, tfp);
    run_case(dut, { 3, { 3, 1, 2, 5, 4 } }, tfp);
    run_case(dut, { 4, { 4, 3, 2, 1 } }, tfp);
    run_case(dut, { 2, { -1, 10, -5, 7 } }, tfp);
    run_case(dut, { 5, { 1, 2, 3 } }, tfp);
    run_case(dut, { 3, { 7, 7, 7, 7 } }, tfp);

    std::cout << "All single request tests passed." << std::endl;

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}
