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
    struct Color
    {
        std::uint8_t r{}, g{}, b{};
    };

    struct Nucleus
    {
        double x{};
        double y{};
        double birthTime{};
        double velocity{};
        Color color{};
    };

    Color hsvToRgb(double h, double s, double v)
    {
        h = std::fmod(h, 360.0);
        if (h < 0.0) h += 360.0;

        const double c = v * s;
        const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
        const double m = v - c;

        double rr = 0.0, gg = 0.0, bb = 0.0;
        if (h < 60.0)       { rr = c; gg = x; }
        else if (h < 120.0) { rr = x; gg = c; }
        else if (h < 180.0) { gg = c; bb = x; }
        else if (h < 240.0) { gg = x; bb = c; }
        else if (h < 300.0) { rr = x; bb = c; }
        else                { rr = c; bb = x; }

        auto cv = [m](double q) {
            return static_cast<std::uint8_t>(std::clamp((q + m) * 255.0, 0.0, 255.0));
        };
        return {cv(rr), cv(gg), cv(bb)};
    }

    struct Simulation
    {
        int width{};
        int height{};
        double time{};
        double nucleationRate{0.000012}; // nuclei / pixel / second
        double minVelocity{35.0};        // pixels / second
        double maxVelocity{65.0};
        std::mt19937 rng;

        std::vector<Nucleus> nuclei;
        std::vector<int> owner;
        std::vector<double> arrival;

        Simulation(int w, int h, std::uint32_t seed)
            : width(w), height(h), rng(seed),
              owner(static_cast<std::size_t>(w) * h, -1),
              arrival(static_cast<std::size_t>(w) * h, std::numeric_limits<double>::infinity())
        {
        }

        void reset(std::uint32_t seed)
        {
            time = 0.0;
            nuclei.clear();
            std::fill(owner.begin(), owner.end(), -1);
            std::fill(arrival.begin(), arrival.end(), std::numeric_limits<double>::infinity());
            rng.seed(seed);
        }

        void addNuclei(double oldTime, double newTime)
        {
            const double expected = nucleationRate * width * height * (newTime - oldTime);
            const auto safeMean = std::clamp(expected, 0.0, 10000.0);
            std::poisson_distribution<int> countDistribution(safeMean);
            const int count = countDistribution(rng);

            std::uniform_real_distribution<double> xDistribution(0.0, static_cast<double>(width));
            std::uniform_real_distribution<double> yDistribution(0.0, static_cast<double>(height));
            std::uniform_real_distribution<double> velocityDistribution(minVelocity, maxVelocity);
            std::uniform_real_distribution<double> birthDistribution(oldTime, newTime);
            std::uniform_real_distribution<double> hueDistribution(0.0, 360.0);

            for (int i = 0; i < count; ++i)
            {
                Nucleus n;
                n.x = xDistribution(rng);
                n.y = yDistribution(rng);
                n.birthTime = birthDistribution(rng);
                n.velocity = velocityDistribution(rng);
                n.color = hsvToRgb(hueDistribution(rng), 0.68, 0.95);
                nuclei.push_back(n);
            }
        }

        void updateNucleus(const Nucleus& n, double oldTime, double newTime, int grainId)
        {
            if (newTime <= n.birthTime) return;

            const double t0 = std::max(oldTime, n.birthTime);
            const double innerRadius = std::max(0.0, (t0 - n.birthTime) * n.velocity);
            const double outerRadius = (newTime - n.birthTime) * n.velocity;
            if (outerRadius <= innerRadius) return;

            const double inner2 = innerRadius * innerRadius;
            const double outer2 = outerRadius * outerRadius;

            const int yMin = std::max(0, static_cast<int>(std::floor(n.y - outerRadius)));
            const int yMax = std::min(height - 1, static_cast<int>(std::ceil(n.y + outerRadius)));

            auto visitRange = [&](int y, int xMin, int xMax) {
                xMin = std::max(0, xMin);
                xMax = std::min(width - 1, xMax);
                if (xMin > xMax) return;

                const double dy = (static_cast<double>(y) + 0.5) - n.y;
                const double dy2 = dy * dy;

                for (int x = xMin; x <= xMax; ++x)
                {
                    const double dx = (static_cast<double>(x) + 0.5) - n.x;
                    const double d2 = dx * dx + dy2;

                    if (d2 > outer2 || d2 <= inner2) continue;

                    const double arrivalTime = n.birthTime + std::sqrt(d2) / n.velocity;
                    if (arrivalTime > newTime) continue;

                    const std::size_t index = static_cast<std::size_t>(y) * width + x;
                    if (arrivalTime < arrival[index])
                    {
                        arrival[index] = arrivalTime;
                        owner[index] = grainId;
                    }
                }
            };

            for (int y = yMin; y <= yMax; ++y)
            {
                const double dy = (static_cast<double>(y) + 0.5) - n.y;
                const double dy2 = dy * dy;
                if (dy2 > outer2) continue;

                const double outerDx = std::sqrt(std::max(0.0, outer2 - dy2));
                double innerDx = 0.0;
                if (dy2 < inner2)
                    innerDx = std::sqrt(std::max(0.0, inner2 - dy2));

                // Convert continuous pixel-centre intervals to integer pixel indices.
                const int leftOuter = static_cast<int>(std::ceil(n.x - outerDx - 0.5));
                const int leftInner = static_cast<int>(std::floor(n.x - innerDx - 0.5));
                const int rightInner = static_cast<int>(std::ceil(n.x + innerDx - 0.5));
                const int rightOuter = static_cast<int>(std::floor(n.x + outerDx - 0.5));

                visitRange(y, leftOuter, leftInner);
                visitRange(y, rightInner, rightOuter);
            }
        }

        void step(double dt)
        {
            if (!std::isfinite(dt) || dt <= 0.0) return;
            dt = std::min(dt, 0.25);

            const double oldTime = time;
            const double newTime = time + dt;

            addNuclei(oldTime, newTime);

            for (std::size_t i = 0; i < nuclei.size(); ++i)
                updateNucleus(nuclei[i], oldTime, newTime, static_cast<int>(i));

            time = newTime;
        }

        double coverage() const
        {
            const auto filled = std::count_if(owner.begin(), owner.end(), [](int v) { return v >= 0; });
            return owner.empty() ? 0.0 : static_cast<double>(filled) / static_cast<double>(owner.size());
        }

        void render(std::uint8_t* destination, int stride) const
        {
            if (!destination || stride < width * 4) return;

            constexpr std::uint8_t bgR = 17, bgG = 21, bgB = 29;
            constexpr std::uint8_t edgeR = 8, edgeG = 10, edgeB = 15;

            for (int y = 0; y < height; ++y)
            {
                auto* row = destination + static_cast<std::ptrdiff_t>(y) * stride;

                for (int x = 0; x < width; ++x)
                {
                    const std::size_t index = static_cast<std::size_t>(y) * width + x;
                    const int grain = owner[index];

                    std::uint8_t r = bgR, g = bgG, b = bgB;

                    if (grain >= 0)
                    {
                        const bool boundary =
                            (x > 0 && owner[index - 1] != grain) ||
                            (x + 1 < width && owner[index + 1] != grain) ||
                            (y > 0 && owner[index - width] != grain) ||
                            (y + 1 < height && owner[index + width] != grain);

                        if (boundary)
                        {
                            r = edgeR; g = edgeG; b = edgeB;
                        }
                        else
                        {
                            const Color c = nuclei[static_cast<std::size_t>(grain)].color;
                            r = c.r; g = c.g; b = c.b;
                        }
                    }

                    const int p = x * 4;
                    row[p + 0] = r;
                    row[p + 1] = g;
                    row[p + 2] = b;
                    row[p + 3] = 255;
                }
            }
        }
    };
}

MS_Simulation MS_CALL ms_create(int width, int height, std::uint32_t seed)
{
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return nullptr;
    try { return new Simulation(width, height, seed); }
    catch (...) { return nullptr; }
}

void MS_CALL ms_destroy(MS_Simulation simulation)
{
    delete static_cast<Simulation*>(simulation);
}

void MS_CALL ms_reset(MS_Simulation simulation, std::uint32_t seed)
{
    if (simulation) static_cast<Simulation*>(simulation)->reset(seed);
}

void MS_CALL ms_set_parameters(MS_Simulation simulation, double rate, double minV, double maxV)
{
    if (!simulation) return;
    auto& s = *static_cast<Simulation*>(simulation);

    if (std::isfinite(rate)) s.nucleationRate = std::clamp(rate, 0.0, 0.001);
    if (std::isfinite(minV)) s.minVelocity = std::clamp(minV, 0.1, 10000.0);
    if (std::isfinite(maxV)) s.maxVelocity = std::clamp(maxV, s.minVelocity, 10000.0);
}

void MS_CALL ms_step(MS_Simulation simulation, double dt)
{
    if (simulation) static_cast<Simulation*>(simulation)->step(dt);
}

void MS_CALL ms_render_rgba(MS_Simulation simulation, std::uint8_t* destination, int stride)
{
    if (simulation) static_cast<Simulation*>(simulation)->render(destination, stride);
}

int MS_CALL ms_get_nucleus_count(MS_Simulation simulation)
{
    return simulation ? static_cast<int>(static_cast<Simulation*>(simulation)->nuclei.size()) : 0;
}

double MS_CALL ms_get_simulation_time(MS_Simulation simulation)
{
    return simulation ? static_cast<Simulation*>(simulation)->time : 0.0;
}

double MS_CALL ms_get_coverage(MS_Simulation simulation)
{
    return simulation ? static_cast<Simulation*>(simulation)->coverage() : 0.0;
}
