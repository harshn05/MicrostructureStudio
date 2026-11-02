#pragma once

#include <cstdint>

#if defined(_WIN32)

#if defined(MICROSTRUCTURECORE_EXPORTS)

#define MS_API __declspec(dllexport)

#else

#define MS_API __declspec(dllimport)

#endif

#define MS_CALL __cdecl

#else

#define MS_API __attribute__((visibility("default")))

#define MS_CALL

#endif

#ifdef __cplusplus

extern "C"

{
#endif

    typedef void* MS_Simulation;

    MS_API MS_Simulation MS_CALL ms_create(int, int, std::uint32_t);

    MS_API void MS_CALL ms_destroy(MS_Simulation);

    MS_API void MS_CALL ms_reset(MS_Simulation, std::uint32_t);

    MS_API void MS_CALL ms_set_parameters(MS_Simulation, double, double, double);

    MS_API void MS_CALL ms_step(MS_Simulation, double);

    MS_API void MS_CALL ms_render_rgba(MS_Simulation, std::uint8_t*, int);

    MS_API int MS_CALL ms_get_nucleus_count(MS_Simulation);

    MS_API double MS_CALL ms_get_simulation_time(MS_Simulation);

    MS_API double MS_CALL ms_get_coverage(MS_Simulation);

#ifdef __cplusplus
}

#endif