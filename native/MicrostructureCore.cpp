#include "MicrostructureCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace
{

struct Grain
{
    double x;
    double y;
    double birth;
    double v;

    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

struct Sim
{
    int w;
    int h;

    double t = 0.0;
    double rate = 0.000012;
    double vmin = 35.0;
    double vmax = 65.0;

    std::mt19937 rng;

    std::vector<Grain> grains;
    std::vector<int> owner;
    std::vector<double> arrival;

    // Number of pixels that have been assigned to a grain at least once.
    std::size_t transformedPixels = 0;

    Sim(int W, int H, unsigned seed)
        : w(W), h(H), rng(seed), owner(static_cast<std::size_t>(W) * H, -1),
          arrival(static_cast<std::size_t>(W) * H, std::numeric_limits<double>::infinity())
    {
    }

    void reset(unsigned seed)
    {
        t = 0.0;
        grains.clear();

        std::fill(owner.begin(), owner.end(), -1);
        std::fill(arrival.begin(), arrival.end(), std::numeric_limits<double>::infinity());

        transformedPixels = 0;

        rng.seed(seed);
    }

    double coverage() const
    {
        if (owner.empty())
            return 0.0;

        return static_cast<double>(transformedPixels) / static_cast<double>(owner.size());
    }

    void step(double dt)
    {
        // Stop simulation after complete transformation.
        if (coverage() >= 1.0)
            return;

        if (!std::isfinite(dt) || dt <= 0.0)
            return;

        dt = std::min(dt, 0.25);

        const double old = t;
        const double now = t + dt;

        // Generate new nuclei using a Poisson process.
        const double expectedNuclei = std::clamp(rate * w * h * dt, 0.0, 10000.0);

        std::poisson_distribution<int> poisson(expectedNuclei);
        const int newNuclei = poisson(rng);

        std::uniform_real_distribution<double> xd(0.0, w);
        std::uniform_real_distribution<double> yd(0.0, h);
        std::uniform_real_distribution<double> vd(vmin, vmax);
        std::uniform_real_distribution<double> hd(0.0, 360.0);
        std::uniform_real_distribution<double> bd(old, now);

        // Create new grains.
        for (int k = 0; k < newNuclei; ++k)
        {
            const double hue = hd(rng);
            const double c = 0.95 * 0.68;
            const double x = c * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
            const double m = 0.95 - c;

            double R = 0.0;
            double G = 0.0;
            double B = 0.0;

            if (hue < 60.0)
            {
                R = c;
                G = x;
            }
            else if (hue < 120.0)
            {
                R = x;
                G = c;
            }
            else if (hue < 180.0)
            {
                G = c;
                B = x;
            }
            else if (hue < 240.0)
            {
                G = x;
                B = c;
            }
            else if (hue < 300.0)
            {
                R = x;
                B = c;
            }
            else
            {
                R = c;
                B = x;
            }

            const auto toByte = [m](double value)
            {
                return static_cast<std::uint8_t>(
                    std::clamp((value + m) * 255.0, 0.0, 255.0));
            };

            grains.push_back({xd(rng), yd(rng), bd(rng), vd(rng), toByte(R), toByte(G), toByte(B)});
        }

        // Update grain growth and pixel ownership.
        for (std::size_t id = 0; id < grains.size(); ++id)
        {
            const auto& grain = grains[id];

            if (now <= grain.birth)
                continue;

            const double innerRadius =
                std::max(0.0, (std::max(old, grain.birth) - grain.birth) * grain.v);
            const double outerRadius = (now - grain.birth) * grain.v;

            const int y0 = std::max(0, static_cast<int>(std::floor(grain.y - outerRadius)));
            const int y1 = std::min(h - 1, static_cast<int>(std::ceil(grain.y + outerRadius)));

            for (int y = y0; y <= y1; ++y)
            {
                const int x0 = std::max(0, static_cast<int>(std::floor(grain.x - outerRadius)));
                const int x1 = std::min(w - 1, static_cast<int>(std::ceil(grain.x + outerRadius)));

                for (int x = x0; x <= x1; ++x)
                {
                    const double dx = x + 0.5 - grain.x;
                    const double dy = y + 0.5 - grain.y;
                    const double distanceSquared = dx * dx + dy * dy;

                    if (distanceSquared <= innerRadius * innerRadius ||
                        distanceSquared > outerRadius * outerRadius)
                    {
                        continue;
                    }

                    const double arrivalTime = grain.birth + std::sqrt(distanceSquared) / grain.v;
                    const std::size_t pixel = static_cast<std::size_t>(y) * w + x;

                    if (arrivalTime < arrival[pixel])
                    {
                        // Count a pixel only when it is transformed for the first time.
                        if (owner[pixel] == -1)
                            ++transformedPixels;

                        arrival[pixel] = arrivalTime;
                        owner[pixel] = static_cast<int>(id);
                    }
                }
            }
        }

        t = now;
    }

    void render(std::uint8_t* dst, int stride) const
    {
        if (!dst || stride < w * 4)
            return;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const std::size_t pixel = static_cast<std::size_t>(y) * w + x;
                const int grainId = owner[pixel];
                auto* q = dst + static_cast<std::size_t>(y) * stride + x * 4;

                std::uint8_t r = 17;
                std::uint8_t g = 21;
                std::uint8_t b = 29;

                if (grainId >= 0)
                {
                    const bool edge = (x > 0 && owner[pixel - 1] != grainId) ||
                                      (x + 1 < w && owner[pixel + 1] != grainId) ||
                                      (y > 0 && owner[pixel - w] != grainId) ||
                                      (y + 1 < h && owner[pixel + w] != grainId);

                    if (edge)
                    {
                        r = 8;
                        g = 10;
                        b = 15;
                    }
                    else
                    {
                        const auto& grain = grains[grainId];
                        r = grain.r;
                        g = grain.g;
                        b = grain.b;
                    }
                }

                q[0] = r;
                q[1] = g;
                q[2] = b;
                q[3] = 255;
            }
        }
    }
};

} // namespace

// -----------------------------------------------------------------------------
// C API
// -----------------------------------------------------------------------------

MS_Simulation MS_CALL ms_create(int w, int h, std::uint32_t seed)
{
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
        return nullptr;

    try
    {
        return new Sim(w, h, seed);
    }
    catch (...)
    {
        return nullptr;
    }
}

void MS_CALL ms_destroy(MS_Simulation p)
{
    delete static_cast<Sim*>(p);
}

void MS_CALL ms_reset(MS_Simulation p, std::uint32_t seed)
{
    if (p)
        static_cast<Sim*>(p)->reset(seed);
}

void MS_CALL ms_set_parameters(MS_Simulation p, double rate, double minVelocity, double maxVelocity)
{
    if (!p)
        return;

    auto& sim = *static_cast<Sim*>(p);

    if (std::isfinite(rate))
        sim.rate = std::clamp(rate, 0.0, 0.001);

    if (std::isfinite(minVelocity))
        sim.vmin = std::clamp(minVelocity, 0.1, 10000.0);

    if (std::isfinite(maxVelocity))
        sim.vmax = std::clamp(maxVelocity, sim.vmin, 10000.0);
}

void MS_CALL ms_step(MS_Simulation p, double dt)
{
    if (p)
        static_cast<Sim*>(p)->step(dt);
}

void MS_CALL ms_render_rgba(MS_Simulation p, std::uint8_t* dst, int stride)
{
    if (p)
        static_cast<Sim*>(p)->render(dst, stride);
}

int MS_CALL ms_get_nucleus_count(MS_Simulation p)
{
    if (!p)
        return 0;

    return static_cast<int>(static_cast<Sim*>(p)->grains.size());
}

double MS_CALL ms_get_simulation_time(MS_Simulation p)
{
    return p ? static_cast<Sim*>(p)->t : 0.0;
}

double MS_CALL ms_get_coverage(MS_Simulation p)
{
    return p ? static_cast<Sim*>(p)->coverage() : 0.0;
}
