# Microstructure Studio — Actionable TODO List

This checklist consolidates all scientific, performance, UI, ABI, and repository hygiene improvements identified during the code review. Items are ordered by priority and dependency.

---

## 🔴 Priority 1: Physics & Scientific Correctness (JMAK Nucleation)

- [ ] **Prevent phantom nucleation in already-transformed regions**
  - **Issue:** Currently, nuclei spawn uniformly across the entire domain even if the nucleation site has already been swept by an earlier grain (~52% of nuclei own 0 pixels, inflating the nucleus count by ~2×).
  - **Fix in `native/MicrostructureCore.cpp`:**
    1. Generate candidate nuclei for the step `[old, now]` and sort them in ascending order of `birth` time.
    2. Before accepting a candidate nucleus at `(x, y)`, map it to its containing pixel index and check `arrival[pixel] <= candidate.birth`.
    3. If already transformed at or before `candidate.birth`, reject the nucleus (do not push to `grains`).
    4. Grow accepted nuclei in chronological birth order so intra-step competition remains exact.
  - **Optional toggle:** Consider exposing a mode flag (`KineticVoronoi` vs. `JmakPhantomFree`) if you want to preserve the pure kinetic Voronoi reference model alongside JMAK nucleation.

---

## 🟠 Priority 2: Native Engine Performance (5–7× Speedup)

- [ ] **Skip both outer-circle corners and inner-hole spans during row scanning**
  - **File:** `native/MicrostructureCore.cpp` (`Sim::step`)
  - **Fix:** Inside the `for (int y = y0; y <= y1; ++y)` loop:
    1. Compute exact outer chord half-width: `maxDx = std::sqrt(outerRadius * outerRadius - dy * dy)` to clip `[x0, x1]` to the circle rather than its bounding square (~21.5% fewer checks).
    2. Compute inner chord half-width: `minDx = std::sqrt(innerRadius * innerRadius - dy * dy)` when `|dy| < innerRadius` and jump directly across `[grain.x - minDx, grain.x + minDx]` instead of testing interior pixels one by one.
- [ ] **Retire grains that cover the entire domain**
  - **File:** `native/MicrostructureCore.cpp`
  - **Fix:** Add a `bool retired = false` flag to `Grain`. Once `innerRadius` exceeds the maximum Euclidean distance from `(grain.x, grain.y)` to all four domain corners `(0, 0)`, `(w, 0)`, `(0, h)`, `(w, h)`, mark `grain.retired = true` and skip it in all subsequent steps.
- [ ] **Defer `std::sqrt` until after distance-squared rejection**
  - **File:** `native/MicrostructureCore.cpp`
  - **Fix:** Compare `distanceSquared` against `((arrival[pixel] - grain.birth) * grain.v)^2` before calling `std::sqrt(distanceSquared)`.

---

## 🟡 Priority 3: C ABI Safety & Exception Guards

- [ ] **Make `MicrostructureCore.h` valid C11 as well as C++17**
  - **File:** `native/MicrostructureCore.h`
  - **Fix:** Replace `#include <cstdint>` and `std::uint32_t` / `std::uint8_t` with `#include <stdint.h>` and bare `uint32_t` / `uint8_t`. Add descriptive parameter names to all exported function prototypes.
- [ ] **Guard `ms_step` and `ms_reset` against C++ exceptions crossing the C ABI**
  - **File:** `native/MicrostructureCore.cpp`
  - **Fix:** Wrap `ms_step` (which allocates via `std::vector::push_back`) in a `try { ... } catch (...)` block so `std::bad_alloc` cannot unwind across `extern "C"` and crash the .NET host.
- [ ] **Prevent integer overflow in `ms_get_nucleus_count`**
  - **File:** `native/MicrostructureCore.cpp`
  - **Fix:** Clamp `grains.size()` to `std::numeric_limits<int>::max()` (or widen the return type to `int64_t` / `long` in a future ABI revision).

---

## 🟢 Priority 4: Managed UI & UX Fixes (`managed/`)

- [ ] **Fix overwritten validation error in `ApplyDimensions()`**
  - **File:** `managed/MainWindow.axaml.cs`
  - **Issue:** `ApplyDimensions()` calls `ApplyParameters()`, then immediately overwrites `StatusText.Text` with `"New simulation created..."` even if parameter parsing failed.
  - **Fix:** Have `ApplyParameters()` return a `bool` indicating success, and only show `"New simulation created..."` if it succeeds.
- [ ] **Sync UI status bar with native parameter clamping**
  - **File:** `managed/MainWindow.axaml.cs`
  - **Issue:** Native `ms_set_parameters` silently clamps `rate` to `[0, 0.001]` and `velocity` to `[0.1, 10000]`, while the UI displays the raw unclamped numbers typed by the user.
  - **Fix:** Either validate against the same bounds in C# before calling `SetParameters` or expose `ms_get_parameters` / clamp in C# so the displayed status matches actual simulation values.
- [ ] **Switch frame timing from `DateTime.UtcNow` to `Stopwatch`**
  - **File:** `managed/MainWindow.axaml.cs`
  - **Fix:** Replace `DateTime.UtcNow` with `Stopwatch.GetTimestamp()` and `Stopwatch.GetElapsedTime(...)` for monotonic, system-clock-independent `dt` measurement.
- [ ] **Add reproducible Seed input/display in the UI**
  - **Files:** `managed/MainWindow.axaml`, `managed/MainWindow.axaml.cs`
  - **Fix:** Add a numeric `Seed` input box (with a "Randomize" button or auto-seed option) and display the active seed in the status bar so runs can be reproduced and shared.
- [ ] **Migrate P/Invoke to `[LibraryImport]`**
  - **File:** `managed/NativeMethods.cs`
  - **Fix:** Replace `[DllImport]` with source-generated `[LibraryImport]` (supported natively in .NET 10) for faster startup and NativeAOT readiness.
- [ ] **Move simulation stepping off the UI thread (Architectural milestone)**
  - **File:** `managed/MainWindow.axaml.cs`
  - **Fix:** Run `ms_step` on a background worker thread with a double-buffered pixel array or synchronization lock so large resolutions (`2048×2048` and `4096×4096`) keep the Avalonia UI responsive at 60 FPS.

---

## 🔵 Priority 5: Automated Testing & CI

- [ ] **Add native CTest verification suite (`tests/`)**
  - **Tests to include:**
    1. **Brute-force oracle test:** Compare `owner` and `arrival` arrays on small grids (`100×100`) across multiple seeds and step sizes (`dt = 0.01` to `0.25`) against an exact analytical reference loop.
    2. **Determinism test:** Verify identical seeds and `dt` sequences produce bit-identical state.
    3. **Boundary & ABI safety test:** Verify null handles, invalid dimensions (`0`, `-1`, `9000`), `NaN`/`Inf` parameters, and zero-stride buffers fail gracefully without crashing.
    4. **Benchmark harness:** Measure step time vs. render time across `800×600`, `1600×1600`, and `2048×2048`.
- [ ] **Add GitHub Actions CI workflow (`.github/workflows/ci.yml`)**
  - Build native library + run `ctest` on Windows (MSVC & MinGW) and Linux (GCC/Clang), plus `dotnet build` for the Avalonia project.

---

## ⚪ Priority 6: Repository & Documentation Hygiene

- [ ] **Remove temporary bundle artifact**
  - Delete `repo_bundle.txt` from the repository root and add `repo_bundle.txt` to `.gitignore`.
- [ ] **Clean up duplicate batch scripts in `scripts/`**
  - Remove `scripts/configure_mingw.bat` (duplicate of `build-mingw.bat`).
  - Either remove `scripts/run.bat` or update it to auto-detect `build_msvc` or `build_mingw`.
- [ ] **Relocate or prune `state.md`**
  - Move `state.md` from the repository root to `docs/technical-guide.md` (and remove stale commit-hash references like `cfeb335`) so the root directory stays clean.
- [ ] **Fix minor documentation discrepancies**
  - Update Avalonia version in `README.md` from `12.1.4` to `12.1.5` to match `managed/MicrostructureStudio.csproj`.
  - Replace `\( ... \)` inline math delimiters in `docs/mathematical-model.md` with `$ ... $` so GitHub Markdown renders all equations properly.