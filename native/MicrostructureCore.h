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
extern "C" {
#endif

typedef void* MS_Simulation;

MS_API MS_Simulation MS_CALL ms_create(int width, int height, std::uint32_t seed);
MS_API void MS_CALL ms_destroy(MS_Simulation simulation);
MS_API void MS_CALL ms_reset(MS_Simulation simulation, std::uint32_t seed);

MS_API void MS_CALL ms_set_parameters(
    MS_Simulation simulation,
    double nucleation_rate,
    double min_growth_velocity,
    double max_growth_velocity);

MS_API void MS_CALL ms_step(MS_Simulation simulation, double delta_time);
MS_API void MS_CALL ms_render_rgba(MS_Simulation simulation, std::uint8_t* destination, int stride);

MS_API int MS_CALL ms_get_nucleus_count(MS_Simulation simulation);
MS_API double MS_CALL ms_get_simulation_time(MS_Simulation simulation);
MS_API double MS_CALL ms_get_coverage(MS_Simulation simulation);

#ifdef __cplusplus
}
#endif
