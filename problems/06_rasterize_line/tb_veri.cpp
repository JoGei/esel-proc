#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VSolution.h"

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <vector>

struct Point
{
    int8_t x;
    int8_t y;
};

struct Pixel
{
    int8_t x;
    int8_t y;
    bool last;
};

struct LineCase
{
    Point p0;
    Point p1;
};

static std::vector<Pixel> rasterize_line_ref(Point p0, Point p1)
{
    std::vector<Pixel> out;

    int32_t x0 = p0.x;
    int32_t y0 = p0.y;
    int32_t x1 = p1.x;
    int32_t y1 = p1.y;

    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;

    int32_t sx = (dx > 0) - (dx < 0);
    int32_t sy = (dy > 0) - (dy < 0);

    int32_t adx = dx >= 0 ? dx : -dx;
    int32_t ady = dy >= 0 ? dy : -dy;

    int32_t x = x0;
    int32_t y = y0;

    out.push_back({ static_cast<int8_t>(x), static_cast<int8_t>(y), dx == 0 && dy == 0 });

    if (adx >= ady)
    {
        int32_t err = 0;
        for (int32_t i = 0; i < adx; ++i)
        {
            x += sx;
            err += ady;

            if ((err << 1) > adx)
            {
                y += sy;
                err -= adx;
            }

            out.push_back({ static_cast<int8_t>(x), static_cast<int8_t>(y), i == (adx - 1) });
        }
    }
    else
    {
        int32_t err = 0;
        for (int32_t i = 0; i < ady; ++i)
        {
            y += sy;
            err += adx;

            if ((err << 1) > ady)
            {
                x += sx;
                err -= ady;
            }

            out.push_back({ static_cast<int8_t>(x), static_cast<int8_t>(y), i == (ady - 1) });
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

static uint8_t pack_i8(int32_t value)
{
    return static_cast<uint8_t>(static_cast<int8_t>(value));
}

static int8_t unpack_i8(uint32_t value)
{
    return static_cast<int8_t>(value & 0xFFu);
}

static void send_operands_one_cycle(VSolution *dut, const LineCase &line)
{
    dut->i_valid = 1;
    dut->i_payload_p0_1 = pack_i8(line.p0.x);
    dut->i_payload_p0_2 = pack_i8(line.p0.y);
    dut->i_payload_p1_1 = pack_i8(line.p1.x);
    dut->i_payload_p1_2 = pack_i8(line.p1.y);
}

static void clear_input(VSolution *dut)
{
    dut->i_valid = 0;
    dut->i_payload_p0_1 = 0;
    dut->i_payload_p0_2 = 0;
    dut->i_payload_p1_1 = 0;
    dut->i_payload_p1_2 = 0;
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

static void send_line(VSolution *dut, const LineCase &line, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "> [send operands] started." << std::endl;

    wait_ready(dut, tfp, 10000);
    send_operands_one_cycle(dut, line);
    tick(dut, tfp);
    clear_input(dut);

    std::cout << "> [send operands] completed." << std::endl;
}

static std::vector<Pixel> recv_pixels_until_last(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 500000)
{
    std::cout << "> [recv result] started." << std::endl;

    std::vector<Pixel> got;
    bool prev_o_valid = dut->o_valid;

    for (int i = 0; i < max_cycles; ++i)
    {
        tick(dut, tfp);

        bool cur_o_valid = dut->o_valid;
        if (cur_o_valid && !prev_o_valid)
        {
            Pixel px = { unpack_i8(dut->o_payload_fragment_1), unpack_i8(dut->o_payload_fragment_2),
                         dut->o_payload_last != 0 };
            got.push_back(px);

            if (px.last)
            {
                std::cout << "> [recv result] completed." << std::endl;
                return got;
            }
        }

        prev_o_valid = cur_o_valid;
    }

    std::stringstream ss;
    ss << "timeout waiting for final output after " << max_cycles << " cycles";
    fail(dut, tfp, ss.str().c_str());
    return {};
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

static std::string pixel_to_string(const Pixel &px)
{
    std::stringstream ss;
    ss << "(" << static_cast<int>(px.x) << ", " << static_cast<int>(px.y) << ", last=" << px.last << ")";
    return ss.str();
}

static void expect_pixels(VSolution *dut, const LineCase &line, const std::vector<Pixel> &expected,
                          VerilatedVcdC *tfp = nullptr)
{
    send_line(dut, line, tfp);
    std::vector<Pixel> got = recv_pixels_until_last(dut, tfp);

    std::cout << "  expected count = " << expected.size() << std::endl;
    std::cout << "  got count      = " << got.size() << std::endl;

    if (got.size() != expected.size())
    {
        std::stringstream ss;
        ss << "pixel count mismatch: got=" << got.size() << " expected=" << expected.size();
        fail(dut, tfp, ss.str().c_str());
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        std::cout << "  pixel[" << i << "] got=" << pixel_to_string(got[i]) << " exp=" << pixel_to_string(expected[i])
                  << std::endl;

        if (got[i].x != expected[i].x || got[i].y != expected[i].y || got[i].last != expected[i].last)
        {
            std::stringstream ss;
            ss << "pixel mismatch at index " << i << ": got=" << pixel_to_string(got[i])
               << " expected=" << pixel_to_string(expected[i]);
            fail(dut, tfp, ss.str().c_str());
        }
    }

    wait_o_valid_clear(dut, tfp);
}

static void run_case(VSolution *dut, const LineCase &line, VerilatedVcdC *tfp = nullptr)
{
    std::cout << "Test: p0=(" << static_cast<int>(line.p0.x) << ", " << static_cast<int>(line.p0.y) << ") p1=("
              << static_cast<int>(line.p1.x) << ", " << static_cast<int>(line.p1.y) << ")" << std::endl;

    expect_pixels(dut, line, rasterize_line_ref(line.p0, line.p1), tfp);
}

static void run_streaming_test(VSolution *dut, VerilatedVcdC *tfp = nullptr, int max_cycles = 1500000)
{
    std::vector<LineCase> inputs = { { { 0, 0 }, { 0, 0 } },   { { 0, 0 }, { 5, 0 } },   { { 5, 0 }, { 0, 0 } },
                                     { { 0, 0 }, { 0, 5 } },   { { 0, 5 }, { 0, 0 } },   { { 0, 0 }, { 5, 5 } },
                                     { { -3, -1 }, { 4, 2 } }, { { 2, -4 }, { -1, 3 } }, { { -5, 3 }, { 2, -4 } } };

    std::deque<Pixel> expected_queue;
    for (const auto &line : inputs)
    {
        for (const auto &px : rasterize_line_ref(line.p0, line.p1))
            expected_queue.push_back(px);
    }

    clear_input(dut);
    drain_stale_output(dut, tfp);

    size_t send_idx = 0;
    size_t recv_idx = 0;
    bool prev_o_valid = dut->o_valid;

    for (int cycle = 0; cycle < max_cycles; ++cycle)
    {
        if (send_idx < inputs.size() && dut->i_ready)
        {
            send_operands_one_cycle(dut, inputs[send_idx]);
            std::cout << "[stream] send line " << send_idx << " p0=(" << static_cast<int>(inputs[send_idx].p0.x) << ", "
                      << static_cast<int>(inputs[send_idx].p0.y) << ") p1=(" << static_cast<int>(inputs[send_idx].p1.x)
                      << ", " << static_cast<int>(inputs[send_idx].p1.y) << ")" << std::endl;
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
                ss << "unexpected output pixel at recv_idx=" << recv_idx;
                fail(dut, tfp, ss.str().c_str());
            }

            Pixel got = { unpack_i8(dut->o_payload_fragment_1), unpack_i8(dut->o_payload_fragment_2),
                          dut->o_payload_last != 0 };
            Pixel expected = expected_queue.front();
            expected_queue.pop_front();

            std::cout << "[stream] recv pixel[" << recv_idx << "] got=" << pixel_to_string(got)
                      << " exp=" << pixel_to_string(expected) << std::endl;

            if (got.x != expected.x || got.y != expected.y || got.last != expected.last)
            {
                std::stringstream ss;
                ss << "streaming mismatch at pixel " << recv_idx << ": got=" << pixel_to_string(got)
                   << " expected=" << pixel_to_string(expected);
                fail(dut, tfp, ss.str().c_str());
            }

            recv_idx++;
        }

        prev_o_valid = cur_o_valid;

        if (send_idx == inputs.size() && expected_queue.empty())
        {
            clear_input(dut);
            std::cout << "Streaming test passed." << std::endl;
            std::cout << "finished stream after [" << cycle << "] clock cycles." << std::endl;
            return;
        }
    }

    std::stringstream ss;
    ss << "streaming test timeout. sent=" << send_idx << " received=" << recv_idx
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
    tfp->open("tb_rasterize_line.vcd");

    reset_dut(dut, tfp);

    run_case(dut, { { 0, 0 }, { 0, 0 } }, tfp);
    run_case(dut, { { 0, 0 }, { 5, 0 } }, tfp);
    run_case(dut, { { 0, 0 }, { 0, 5 } }, tfp);
    run_case(dut, { { 0, 0 }, { 5, 5 } }, tfp);
    run_case(dut, { { -3, -1 }, { 4, 2 } }, tfp);
    run_case(dut, { { 2, -4 }, { -1, 3 } }, tfp);

    std::cout << "All single request tests passed." << std::endl;

    run_streaming_test(dut, tfp);

    std::cout << "All streaming tests passed." << std::endl;

    tfp->close();
    delete tfp;
    delete dut;
    return 0;
}
