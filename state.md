---
author: Technical Documentation
date: 03 November 2026
subtitle: A Technical Book
title: Microstructure Studio
---

# Microstructure Studio

## A technical guide to the current simulation engine, mathematical model, software architecture and development workflow

**Edition 1**  
**Repository baseline:** `main`  
**Commit:** `cfeb335067d62e0817c09cce010e6a4152f7c4f6`  
**Documentation date:** 03 November 2026

> This book documents the repository at the stated commit. It describes
> the implementation that exists in that snapshot. Proposed capabilities
> and roadmap items are identified separately and are not presented as
> implemented features.

------------------------------------------------------------------------

# How to use this book

This document is a source-grounded technical guide to the current
Microstructure Studio repository. It explains the model, the native code
path, the native/managed boundary, the user interface, build and
deployment, current limitations, and a disciplined roadmap.

It is intended for developers and computational materials researchers
who want to understand the present implementation before extending it.

**Scope discipline:** the repository is the authority for statements
about current behaviour. Where the source does not specify a detail,
this book avoids silently assuming one.

# Contents

1.  [Project overview](#1-project-overview)
2.  [Mathematical model](#2-mathematical-model)
3.  [Native engine internals](#3-native-engine-internals)
4.  [Native C API and managed
    integration](#4-native-c-api-and-managed-integration)
5.  [Avalonia user interface](#5-avalonia-user-interface)
6.  [Build system and deployment](#6-build-system-and-deployment)
7.  [Numerical behaviour, assumptions and
    limitations](#7-numerical-behaviour-assumptions-and-limitations)
8.  [Performance characteristics and optimisation
    opportunities](#8-performance-characteristics-and-optimisation-opportunities)
9.  [Development workflow and repository
    discipline](#9-development-workflow-and-repository-discipline)
10. [Roadmap and proposed research
    extensions](#10-roadmap-and-proposed-research-extensions)
11. [Technical reference](#11-technical-reference)

------------------------------------------------------------------------

# 1. Project overview

Microstructure Studio is a real-time, two-dimensional nucleation and
grain-growth demonstrator. It combines an Avalonia desktop interface
written in C#/.NET with a native C++17 simulation library.

The native engine is intentionally independent of Avalonia and .NET. It
exposes a compact C ABI consumed by the managed application through
P/Invoke.

The project is currently described by its README as an **early-stage
research / educational prototype**. Its purpose is to make continuous
stochastic nucleation and isotropic grain growth visible in an
interactive desktop application.

## 1.1 Implemented capabilities

The current repository implements:

- Continuous stochastic nucleation using a Poisson process.
- Isotropic circular growth with a configurable random velocity
  interval.
- Kinetic Voronoi-like ownership based on earliest arrival time.
- Per-grain colour assignment and pixel-level grain-boundary rendering.
- Live simulation controls, time, grain count and area coverage.
- A native C++ engine exposed through a small C ABI and consumed through
  C# P/Invoke.
- CMake build support for MSVC and MinGW-w64.
- Self-contained .NET publishing for Windows x64.
- Static compiler-runtime linking options for native Windows deployment.

## 1.2 What the current version is not

The current repository explicitly states that OpenMP is not implemented
and that both simulation and rendering execute serially in the native
engine.

The current model is **not**:

- A curvature-driven grain-boundary migration model.
- A phase-field model.
- A crystallographic anisotropic growth model.
- A 3D microstructure simulation.

These distinctions matter when interpreting the generated
microstructures or comparing them with experimental materials.

## 1.3 Repository map

| Path                         | Responsibility                                                                     |
|:-----------------------------|:-----------------------------------------------------------------------------------|
| `README.md`                  | Project overview, features, requirements, build instructions, defaults and roadmap |
| `CMakeLists.txt`             | Top-level CMake configuration and managed publish target                           |
| `native/`                    | C++17 simulation library and exported C API                                        |
| `managed/`                   | Avalonia UI, application startup and P/Invoke declarations                         |
| `docs/architecture.md`       | Native/managed architecture and memory ownership                                   |
| `docs/mathematical-model.md` | Mathematical description and model characteristics                                 |
| `scripts/`                   | MSVC and MinGW configure/build/run helpers                                         |
| `LICENSE`                    | Project MIT licence                                                                |
| `THIRD-PARTY-NOTICES.md`     | Third-party notices                                                                |

## 1.4 High-level architecture

``` text
Avalonia UI / C# / .NET 10
        |
        | P/Invoke (C ABI)
        v
MicrostructureCore.dll / C++17
        |
        +-- Poisson nucleation
        +-- isotropic radial growth
        +-- earliest-arrival pixel ownership
        +-- RGBA framebuffer rendering
        |
        v
Avalonia WriteableBitmap
```

The native library does not depend on Avalonia or .NET. This separation
is intentional so the engine can later be reused by other front ends.

------------------------------------------------------------------------

# 2. Mathematical model

The current engine implements continuous stochastic nucleation followed
by isotropic radial growth at a constant velocity assigned to each
grain. The domain is represented by a rectangular pixel grid. The
continuous growth model is sampled at pixel centres.

## 2.1 Continuous stochastic nucleation

Let:

- $A$ be the simulation-domain area,
- $\lambda$ be the nucleation rate per unit area per unit time,
- $\Delta t$ be the simulation time increment.

The number of nuclei generated during a step is sampled from a Poisson
distribution:

$$
N_{new} \sim \text{Poisson}(\lambda A \Delta t)
$$

In the implementation, the expected number is computed as:

``` cpp
rate * w * h * dt
```

The expected value is clamped to a maximum of 10,000 and passed to
`std::poisson_distribution<int>`.

New nuclei are assigned:

- A random x position.
- A random y position.
- A random birth time within the current interval.
- A random growth velocity between the configured minimum and maximum.
- A colour derived from a random hue.

The nucleation rate is expressed in nuclei / pixel / second for the
pixel-based domain used by the application.

The README default is:

$$
\lambda = 0.000012 \quad \text{nuclei/pixel/s}
$$

## 2.2 Isotropic radial growth

A grain (i) is characterised by:

- Centre $(x_i,y_i)$.
- Birth time $t_i$.
- Constant radial velocity $v_i$.

Its ideal radius at simulation time $t$ is:

$$
R_i(t)=v_i(t-t_i)
$$

This is circular growth in two dimensions. The current implementation
does not introduce crystallographic anisotropy, curvature-dependent
velocity, elastic effects, solute diffusion, or thermal fields.

## 2.3 Arrival-time field

For a point $(x,y)$, the arrival time of grain $i$ is the time at which
its radial front reaches that point:

$$
\tau_i(x,y)=t_i+\frac{\sqrt{(x-x_i)^2+(y-y_i)^2}}{v_i}
$$

The arrival-time expression combines nucleation history and growth
speed. A grain born earlier has a head start; a faster grain reaches
distant locations sooner. Consequently, grain ownership is not
determined by distance to the nearest nucleus alone.

## 2.4 Pixel ownership

The owner of a point is the grain with the smallest arrival time:

$$
G(x,y)=\arg\min_{i} \tau_i(x,y)
$$

The engine evaluates this competition at pixel centres:

$$
(x+0.5,\ y+0.5)
$$

It stores an owner index and the best arrival time for each pixel. When
a grain reaches a pixel with an arrival time smaller than the currently
stored value, the stored arrival and owner are replaced.

## 2.5 Impingement in the current model

The repository describes this earliest-arrival ownership as naturally
accounting for grain impingement. In practical terms, a pixel is
assigned to the grain whose arrival-time function wins at that location.

The current implementation does **not** model a moving interface that
physically collides and then blocks a grain’s front from travelling
through neighbouring grain regions.

This distinction is central to any future alternative growth model:

- In the current kinetic Voronoi-like model, each grain’s arrival-time
  field competes for every pixel.
- In an explicitly impingement-limited model, collision of growth fronts
  would constrain subsequent propagation.

These are different modelling assumptions and should be represented as
separate selectable models if both are required.

## 2.6 Discrete time stepping and swept annulus

For each time step, the engine considers the newly swept radial band
between an inner and outer radius.

The inner radius is based on the later of the previous simulation time
and the grain’s birth time. The outer radius is based on the new
simulation time.

``` cpp
const double innerRadius =
    std::max(0.0, (std::max(old, grain.birth) - grain.birth) * grain.v);

const double outerRadius = (now - grain.birth) * grain.v;
```

Pixels inside the already processed radius are skipped. This avoids
repeatedly processing the full disk for every grain at every step.

The engine first computes a rectangular bounding box around the outer
radius, clipped to the simulation domain. It then tests candidate pixel
centres against the inner and outer squared radii. Only pixels in the
newly swept annulus are considered.

## 2.7 Coverage definition

Coverage is calculated from `transformedPixels`, which counts the number
of pixels that have been assigned to any grain at least once. It is
divided by the total number of pixels in the owner array:

$$
\text{Coverage}=\frac{\text{transformedPixels}}{wh}
$$

It is therefore a measure of whether the domain has been reached by at
least one grain, not a count of pixels currently owned by a particular
grain.

Since ownership may later change when an earlier arrival is discovered,
the counter is deliberately not decremented.

This distinction is important: the simulation’s completion check is
based on first transformation of the full raster domain.

------------------------------------------------------------------------

# 3. Native engine internals

The native implementation is concentrated in
`native/MicrostructureCore.cpp`. It uses standard C++ facilities for
algorithms, mathematics, random distributions and dynamic arrays.

The public interface is declared in `native/MicrostructureCore.h`.

## 3.1 Grain data structure

The current grain structure is:

``` cpp
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
```

Each grain stores its nucleation position, birth time, radial velocity
and RGB colour.

The vector index is used as the grain identifier. The pixel ownership
array stores this index, so grain vector indices must remain stable
while the simulation is running.

## 3.2 Simulation state

The current simulation state contains:

``` cpp
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

    std::size_t transformedPixels = 0;
};
```

The arrays have the following meanings:

| Member              | Meaning                                                           |
|:--------------------|:------------------------------------------------------------------|
| `w`, `h`            | Raster dimensions                                                 |
| `t`                 | Current simulation time                                           |
| `rate`              | Nucleation rate                                                   |
| `vmin`, `vmax`      | Velocity interval                                                 |
| `rng`               | Per-simulation random-number generator                            |
| `grains`            | All nucleated grains                                              |
| `owner`             | Grain index currently owning each pixel; `-1` means untransformed |
| `arrival`           | Best known arrival time for each pixel                            |
| `transformedPixels` | Number of pixels assigned at least once                           |

The `owner` array contains one integer per pixel. A value of `-1`
indicates that no grain has yet transformed that pixel.

The `arrival` array stores the best known arrival time and is
initialised to positive infinity.

The random-number generator is a per-simulation `std::mt19937` engine
seeded at creation or reset.

## 3.3 Reset behaviour

Reset performs the following operations:

1.  Returns simulation time to zero.
2.  Clears the grain vector.
3.  Restores every owner to `-1`.
4.  Restores every arrival time to infinity.
5.  Resets the transformed-pixel count.
6.  Reseeds the random generator.

Reset therefore starts a new stochastic realisation when supplied a new
seed.

## 3.4 Step procedure

A native simulation step follows this sequence:

1.  Return immediately if coverage is already 100%.
2.  Reject non-finite or non-positive time increments.
3.  Clamp the time increment to at most 0.25 seconds.
4.  Record old and new simulation times.
5.  Sample the number of new nuclei from the Poisson distribution.
6.  Create each new grain with random position, birth time, velocity and
    colour.
7.  Iterate over all grains and compute the newly swept annulus.
8.  Scan candidate pixels inside the grain’s clipped bounding box.
9.  Update arrival time and owner when the candidate arrival is earlier.
10. Advance the simulation clock.

## 3.5 Pixel update logic

The central ownership update is:

``` cpp
const double arrivalTime =
    grain.birth + std::sqrt(distanceSquared) / grain.v;

const std::size_t pixel =
    static_cast<std::size_t>(y) * w + x;

if (arrivalTime < arrival[pixel])
{
    if (owner[pixel] == -1)
        ++transformedPixels;

    arrival[pixel] = arrivalTime;
    owner[pixel] = static_cast<int>(id);
}
```

The first-time transformation counter is incremented only when the owner
was previously `-1`.

If a later calculation finds an earlier arrival time and changes
ownership, coverage remains unchanged because that pixel was already
transformed.

## 3.6 Rendering

Rendering is performed by `Sim::render`.

The caller supplies a destination buffer and row stride.

- Untransformed pixels receive a dark background colour.
- Transformed pixels are coloured using their owning grain’s stored RGB
  value.
- Pixels adjacent to a different owner in the four cardinal directions
  are rendered in a dark boundary colour.

The renderer writes RGBA bytes and checks that the destination pointer
and stride are valid.

The current renderer is serial and traverses the complete raster for
every rendered frame.

------------------------------------------------------------------------

# 4. Native C API and managed integration

The native library exposes an opaque simulation handle through a
C-compatible API.

The C ABI avoids exposing C++ classes, templates, standard-library
containers or compiler-specific object layouts to the C# application.

## 4.1 Exported API

The current API in `native/MicrostructureCore.h` is:

| Function                                   | Purpose                                                                            |
|:-------------------------------------------|:-----------------------------------------------------------------------------------|
| `ms_create(w, h, seed)`                    | Allocate a simulation object and its pixel arrays; return an opaque handle or null |
| `ms_destroy(sim)`                          | Destroy the native simulation object                                               |
| `ms_reset(sim, seed)`                      | Reset simulation state and reseed the random generator                             |
| `ms_set_parameters(sim, rate, minV, maxV)` | Set nucleation rate and velocity limits, with native validation/clamping           |
| `ms_step(sim, dt)`                         | Advance the simulation by a time increment                                         |
| `ms_render_rgba(sim, dst, stride)`         | Render the current state into a caller-owned RGBA buffer                           |
| `ms_get_nucleus_count(sim)`                | Return the number of grains currently in the grain vector                          |
| `ms_get_simulation_time(sim)`              | Return the current simulation time                                                 |
| `ms_get_coverage(sim)`                     | Return transformed-pixel coverage                                                  |

## 4.2 Memory ownership

The native engine owns the simulation object and its internal arrays.

The managed application owns the destination bitmap and passes its
locked pixel-buffer address to the native renderer.

The architecture documentation specifies:

- The simulation object is owned by the managed application and must be
  destroyed once.
- The caller owns the destination RGBA buffer.
- The native renderer writes into that caller-owned memory.

This is a simple ownership contract, but it must be respected during
window shutdown and simulation recreation.

## 4.3 P/Invoke declarations

The managed file `managed/NativeMethods.cs` declares each native
function using `DllImport`, the library name `MicrostructureCore.dll`,
and the C calling convention.

The native signatures and managed declarations must remain aligned.

If a new native API is added, the matching C# declaration must be added
before the UI can call it.

## 4.4 Main window lifecycle

The current application lifecycle is:

1.  The window initialises its controls and creates a `DispatcherTimer`
    with an approximately 33 ms interval.
2.  Outside design mode, it creates the native simulation and an
    Avalonia `WriteableBitmap`.
3.  Apply Parameters validates text-box values and passes them to the
    native engine.
4.  Start/Pause toggles the managed running flag and updates the button
    label.
5.  On each timer tick, elapsed wall-clock time is converted to a
    bounded simulation time step.
6.  The native engine advances, the framebuffer is rendered, and status
    text is updated.
7.  Reset reseeds and clears the native simulation, pauses playback and
    renders the empty state.
8.  On window close, the timer is stopped, the native handle is
    destroyed and the bitmap is disposed.

## 4.5 UI thread and performance

The current timer callback performs native stepping and rendering
synchronously from the Avalonia dispatcher timer.

The README roadmap explicitly lists moving simulation work off the UI
thread as future work.

Large domains or high grain counts may therefore affect UI
responsiveness.

Any future background-worker design must carefully coordinate access to
the native simulation and bitmap buffer.

------------------------------------------------------------------------

# 5. Avalonia user interface

The current UI is defined in:

- `managed/MainWindow.axaml`
- `managed/MainWindow.axaml.cs`

It uses a dark interface with a left-side control panel and a large
simulation viewport.

## 5.1 Controls and defaults

| Control          | Current default / behaviour                                        |
|:-----------------|:-------------------------------------------------------------------|
| Domain X         | 800 pixels                                                         |
| Domain Y         | 600 pixels                                                         |
| Nucleation rate  | 0.000012 nuclei/pixel/s                                            |
| Minimum velocity | 35 px/s                                                            |
| Maximum velocity | 65 px/s                                                            |
| Apply resolution | Creates a new native simulation and bitmap                         |
| Apply parameters | Validates and updates rate and velocity bounds                     |
| Start / Pause    | Starts or pauses timer-driven simulation                           |
| Reset            | Clears the current simulation and creates a new random realisation |

The code-behind limits user-entered dimensions to 64–4096 pixels in each
direction.

The native creation API accepts dimensions up to 8192, but the current
UI applies the stricter 4096 limit.

This is an example of validation at two layers: UI constraints and
native safety checks.

## 5.2 Status information

During a run, the status bar displays:

- Simulation time.
- Nucleus count.
- Coverage percentage.

When coverage reaches 100%, the application stops playback and displays
a completion message.

The current version does not display active-grain count or a selected
growth-model mode.

## 5.3 Bitmap rendering path

The managed application locks the Avalonia `WriteableBitmap`, passes its
address and row stride to `ms_render_rgba`, then invalidates the
viewport.

The native renderer therefore writes directly into the bitmap’s locked
memory, avoiding an additional managed-side per-pixel conversion loop.

------------------------------------------------------------------------

# 6. Build system and deployment

The project uses CMake to build the native shared library and to
orchestrate publishing of the managed .NET application.

The top-level CMake file requires CMake 3.21 or newer and C++17.

## 6.1 Build dependencies

- Windows 10/11 x64.
- CMake 3.21 or newer.
- Ninja.
- MSVC x64 or MinGW-w64 GCC with C++17 support.
- .NET 10 SDK.
- NuGet access for the first restore unless packages are already cached.

The managed project currently references Avalonia 12.1.4.

The configured runtime identifier is `win-x64`.

## 6.2 MSVC build

From the repository root:

``` bat
scripts\build-msvc.bat
```

Run the application:

``` bat
scripts\run-msvc.bat
```

The README states that the MSVC Release native library uses the static
MSVC runtime (`/MT`).

## 6.3 MinGW-w64 build

From the repository root:

``` bat
scripts\build-mingw.bat
```

Run the application:

``` bat
scripts\run-mingw.bat
```

The MinGW build requests static GCC and C++ runtime linking through:

``` text
-static
-static-libgcc
-static-libstdc++
```

The repository recommends inspecting the resulting DLL dependencies
rather than assuming every dependency is eliminated.

## 6.4 Direct CMake commands

MSVC:

``` bat
cmake -S . -B build_msvc -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_msvc --target publish_dotnet
```

MinGW:

``` bat
cmake -S . -B build_mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_mingw --target publish_dotnet
```

The `publish_dotnet` target runs `dotnet publish`, then copies the built
native DLL into the publish directory.

By default, publishing is self-contained.

The output directory is:

``` text
build_<toolchain>/dotnet_publish/
```

It contains the application executable, managed dependencies, the
self-contained .NET runtime and `MicrostructureCore.dll`.

## 6.5 Framework-dependent publishing

To create a framework-dependent build instead:

``` bat
-DMS_DOTNET_SELF_CONTAINED=OFF
```

This deployment requires the matching .NET 10 Desktop Runtime on the
target machine.

## 6.6 Deployment checks

For MSVC, inspect dependencies with:

``` bat
dumpbin /dependents MicrostructureCore.dll
```

For MinGW, use:

``` bat
objdump -p MicrostructureCore.dll
```

Windows system DLLs and API-set dependencies are expected.

Before redistributing binaries, review `THIRD-PARTY-NOTICES.md` and
include the full applicable third-party licence texts and runtime
notices.

------------------------------------------------------------------------

# 7. Numerical behaviour, assumptions and limitations

A useful scientific software document should distinguish what the code
computes from what a reader might infer physically.

This chapter summarises the current model’s scope and the limitations
stated or directly visible in the repository.

## 7.1 Assumptions in the current implementation

- Two-dimensional rectangular domain represented by a regular pixel
  grid.
- Continuous stochastic nucleation sampled over discrete time steps.
- Each grain has a fixed nucleation position, birth time and constant
  radial growth velocity.
- Growth is isotropic, so ideal fronts are circular.
- Each pixel is assigned according to the smallest calculated arrival
  time.
- The continuous model is sampled at pixel centres.
- Grain ownership is stored as a single integer index per pixel.
- Simulation and rendering are serial in the current version.

## 7.2 Interpretation of impingement

The project documentation uses the term impingement for the
earliest-arrival ownership partition.

In this formulation, grain regions emerge from competition between
arrival-time fields.

It is not equivalent to explicitly simulating two moving interfaces that
collide and physically block each other’s subsequent propagation.

That distinction motivated the proposed future option for an alternative
impingement-limited model, but no such second model exists in the
reverted baseline.

## 7.3 Time-step dependence

The simulation uses a finite time increment and processes the newly
swept annulus.

New nuclei are assigned birth times sampled within the current interval.
The engine caps the requested increment at 0.25 seconds. The README
describes the UI timer interval as approximately 33 ms.

These are implementation parameters, not a documented convergence study.
The repository does not currently provide a systematic time-step
refinement analysis.

## 7.4 Randomness and reproducibility

The engine uses a seeded `std::mt19937`.

The native API supports explicit seeds at creation and reset. The UI
currently generates a seed using `Random.Shared.Next` when creating or
resetting a simulation.

The UI does not currently expose a seed-entry field. Reproducing a
particular UI run would therefore require recording the seed separately
or adding seed control in a future change.

## 7.5 Coverage is not grain area fraction

Because coverage counts pixels that have ever been assigned, it is not
necessarily equal to the current area fraction of a particular grain.

It also does not report the number of pixels currently owned by each
grain.

Grain area statistics would require a separate count over the current
`owner` array.

## 7.6 Known unimplemented capabilities

| Capability                            | Current status                          |
|:--------------------------------------|:----------------------------------------|
| OpenMP parallelism                    | Not implemented; engine is serial       |
| Spatial bins / neighbour acceleration | Listed as roadmap work; not implemented |
| Active-grain tracking                 | Not implemented in this baseline        |
| Second selectable growth model        | Not implemented in this baseline        |
| Shape-harmonic / anisotropic growth   | Roadmap item                            |
| Grain statistics and data export      | Roadmap item                            |
| Background simulation worker          | Roadmap item                            |
| VTK visualisation / 3D extensions     | Roadmap item                            |

------------------------------------------------------------------------

# 8. Performance characteristics and optimisation opportunities

The current implementation has two major computational paths: simulation
stepping and full-frame rendering.

Both are serial.

The simulation loops over every grain and scans a rectangular region
around its current outer radius, then performs a radial-distance test
for each candidate pixel.

## 8.1 Existing optimisation: swept annulus

The engine does not rescan the entire disk from the nucleus on every
time step.

It computes an inner radius from the previous simulation time and an
outer radius from the new simulation time, and tests only pixels in that
newly swept annulus.

This is an important existing optimisation and must be preserved when
refactoring.

## 8.2 Existing optimisation: per-grain bounding box

For each grain, the engine calculates a square bounding box from its
outer radius, clips it to the domain, and scans only that rectangle.

It still checks the exact squared radial distance for each candidate
pixel.

The bounding box is therefore a broad-phase filter; the
circular-distance test is the narrow-phase geometric test.

## 8.3 Current costs

A rough way to reason about work is to consider, for each grain and each
time step, the area of its bounding square.

The exact number of candidate pixels depends on the grain’s radius and
its intersection with the domain.

As grain count and radius increase, the total candidate work can become
large.

Rendering separately traverses every pixel in the domain for each frame.

## 8.4 Safe optimisation principles

- Keep the current implementation available as a reference path.
- Use deterministic seeds and compare complete owner arrays when
  validating optimisations.
- Separate broad-phase spatial filtering from exact arrival-time
  evaluation.
- Do not permanently deactivate a grain merely because it captured no
  pixel in one time step.
- Do not infer physical enclosure from a raster ring unless the selected
  growth model explicitly defines that rule.
- Measure simulation-step time and render time separately.
- Benchmark multiple domain sizes, nucleation rates and velocity
  distributions.

## 8.5 Candidate future techniques

### Spatial bins

Partition the domain into tiles and associate grains with tiles that
their future growth can reach.

A tile can then consider a smaller candidate set. Any pruning rule must
be conservative: it must not exclude a grain that could still produce an
earlier arrival time at a pixel.

### Scanline or row-wise bounds

The current square bounding box includes corner pixels outside the
circular front.

A row-wise x interval derived from the circle can reduce candidate
checks while retaining the exact annulus and arrival-time tests.

### Parallel execution

OpenMP is a roadmap item, not a current feature.

Parallelising ownership updates requires care because multiple grains
can compete for the same pixel and update the same arrival/owner
entries. A correct implementation needs a deterministic reduction or
another race-free ownership strategy.

### Rendering optimisation

Dirty-region rendering may reduce work when only a small region changes,
but grain-boundary pixels can change when ownership changes.

Any dirty-region strategy must include neighbouring pixels whose edge
status may have changed.

------------------------------------------------------------------------

# 9. Development workflow and repository discipline

The project has separate helper scripts for MSVC and MinGW.

The intended workflow is to open a terminal where the selected compiler,
CMake, Ninja and .NET SDK are already available, then run the
corresponding build script from the repository root.

## 9.1 Recommended baseline workflow

1.  Start from a clean checkout of the chosen commit.
2.  Build the unmodified native engine and managed application.
3.  Record compiler, CMake, Ninja and .NET SDK versions.
4.  Run a baseline simulation at fixed dimensions and parameters.
5.  Record the random seed if deterministic comparison is required.
6.  Make one logically isolated code change.
7.  Rebuild and run focused tests before proceeding.
8.  Compare numerical output with the baseline when changing the
    simulation algorithm.

## 9.2 Why one change at a time matters

The native simulation, C ABI, managed P/Invoke declarations and Avalonia
UI form a connected system.

A change to the native state may require:

- An exported API change.
- A matching managed declaration.
- A UI update.
- Tests for the new behaviour.

Bundling several unrelated changes makes it harder to locate ABI
mismatches, numerical regressions and UI lifecycle errors.

Incremental changes keep the validation surface small.

## 9.3 Suggested test layers

| Layer               | Suggested checks                                                                                                                    |
|:--------------------|:------------------------------------------------------------------------------------------------------------------------------------|
| Native API          | Null handles, invalid dimensions, reset, parameter clamping, finite/non-finite `dt`                                                 |
| Determinism         | Same seed and same sequence of `dt` values produce identical owner and arrival arrays                                               |
| Numerics            | Check ownership against a slow reference implementation on small grids                                                              |
| Coverage            | Coverage remains in $[0,1]$, is monotonic under first-transformation counting, and reaches 1 only when every pixel has been visited |
| Rendering           | Stride handling, null destination, boundary pixels, untransformed background                                                        |
| Managed integration | Create/destroy lifecycle, reset, resize, pause/resume, completion handling                                                          |
| Performance         | Measure step and render time independently across representative scenarios                                                          |

------------------------------------------------------------------------

# 10. Roadmap and proposed research extensions

The repository README lists the following roadmap items. They are future
work and should not be interpreted as features already present in the
current baseline.

## 10.1 OpenMP acceleration and reproducible benchmarks

The README lists OpenMP acceleration and reproducible performance
benchmarks.

A benchmark suite should record:

- Build configuration.
- Compiler and version.
- CPU.
- Grid dimensions.
- Random seed.
- Nucleation rate.
- Velocity interval.
- Simulation duration.
- Grain count.
- Coverage.
- Separate simulation-step and rendering timings.

## 10.2 Shape-harmonic and anisotropic growth

Shape-harmonic growth could provide a route to non-circular grain shapes
and orientation-dependent growth.

It would be a distinct physical/geometric model extension.

The current grain data structure contains only scalar radial velocity
and has no orientation or harmonic coefficients.

## 10.3 Grain statistics and export

Useful future statistics include:

- Current pixel area per grain.
- Equivalent-circle radius.
- Perimeter estimates.
- Neighbour counts.
- Grain-size distributions.
- Time histories.

Export should distinguish current ownership statistics from
transformed-pixel coverage.

## 10.4 Spatial acceleration

Spatial acceleration is explicitly listed in the roadmap.

A correct implementation should preserve earliest-arrival ownership
exactly, or quantify and document any approximation.

A conservative candidate-pruning scheme is preferable to a heuristic
that may silently alter grain ownership.

## 10.5 Background simulation execution

Moving simulation work off the UI thread can improve responsiveness.

It requires a clear threading contract for:

- Native simulation state.
- Cancellation.
- Reset.
- Parameter updates.
- Bitmap transfer.

The UI should not read or render a native simulation concurrently with
an uncoordinated step operation.

## 10.6 Optional VTK visualisation and 3D

The README lists optional VTK-based visualisation and 3D extensions.

The current native engine is strictly two-dimensional and the current
rendering path produces an RGBA raster.

A 3D extension would require changes to the data model, growth geometry,
storage, visualisation and performance strategy.

------------------------------------------------------------------------

# 11. Technical reference

## 11.1 Default simulation parameters

| Parameter        |      Default | Meaning                                         |
|:-----------------|-------------:|:------------------------------------------------|
| Domain           | 800 × 600 px | Raster dimensions in the UI                     |
| Nucleation rate  |     0.000012 | Nuclei per pixel per second                     |
| Minimum velocity |      35 px/s | Lower bound of random grain velocity            |
| Maximum velocity |      65 px/s | Upper bound of random grain velocity            |
| UI timer         |       ~33 ms | DispatcherTimer interval                        |
| Native `dt` cap  |       0.25 s | Maximum time increment accepted per native step |

These are demonstration values, not calibrated material parameters.

## 11.2 Important native implementation limits

- Native `ms_create` rejects non-positive dimensions and dimensions
  greater than 8192.
- The UI currently restricts each dimension to 64–4096.
- Native `ms_step` ignores non-finite or non-positive `dt` and caps `dt`
  at 0.25 seconds.
- Nucleation rate is clamped to $[0, 0.001]$.
- Minimum velocity is clamped to $[0.1, 10000]$.
- Maximum velocity is clamped to at least the current minimum and at
  most 10000.

## 11.3 Key source files

| File                            | What to inspect when…                                         |
|:--------------------------------|:--------------------------------------------------------------|
| `native/MicrostructureCore.cpp` | Changing nucleation, growth, ownership, coverage or rendering |
| `native/MicrostructureCore.h`   | Changing the exported C ABI                                   |
| `managed/NativeMethods.cs`      | Adding or changing P/Invoke declarations                      |
| `managed/MainWindow.axaml`      | Changing the visual layout and controls                       |
| `managed/MainWindow.axaml.cs`   | Changing UI behaviour, timer lifecycle and native calls       |
| `CMakeLists.txt`                | Changing project configuration and publish orchestration      |
| `native/CMakeLists.txt`         | Changing native compiler/linker options                       |

## 11.4 Public repository and documented baseline

Repository:

<https://github.com/harshn05/MicrostructureStudio>

Documented baseline:

- Branch: `main`
- Commit: `cfeb335067d62e0817c09cce010e6a4152f7c4f6`

If the repository changes later, this book should be treated as
documentation of that specific snapshot rather than an automatically
updated manual.

------------------------------------------------------------------------

# Closing note

Microstructure Studio already has a clear and useful foundation: a
compact native engine, a stable C ABI, a managed desktop front end, and
a mathematically stated kinetic growth model.

The safest path forward is to preserve this baseline, add tests that
capture its behaviour, and then introduce optimisations or alternative
physics as isolated, selectable implementations.

**End of technical book · Edition 1 · 03 November 2026**
