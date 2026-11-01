# Architecture

Microstructure Studio separates the scientific model from the desktop presentation.

```text
Avalonia desktop application (C# / .NET 10)
        |
        | P/Invoke, stable C ABI
        v
MicrostructureCore.dll (C++17)
        |
        +-- stochastic nucleation
        +-- isotropic grain growth
        +-- earliest-arrival ownership field
        +-- RGBA framebuffer rendering
        |
        v
Avalonia WriteableBitmap
```

## Managed layer

`managed/` contains the Avalonia application, UI controls, frame timer and P/Invoke declarations. The managed layer owns the window and bitmap.

## Native layer

`native/` builds `MicrostructureCore.dll`. It exposes a small C ABI declared in `MicrostructureCore.h`. The native engine has no Avalonia or .NET dependency.

## ABI and memory ownership

- `ms_create` allocates the opaque simulation object.
- `ms_destroy` releases it.
- `ms_step` advances the simulation.
- `ms_render_rgba` writes into a caller-owned pixel buffer.
- Query functions expose scalar statistics.

The caller owns the destination RGBA buffer. The simulation object is owned by the managed application and must be destroyed once.

## Build and deployment

CMake builds the native DLL with either MSVC or MinGW. The `publish_dotnet` target publishes the managed application for `win-x64` and copies the native DLL beside the executable. The default is self-contained .NET deployment.

MSVC Release builds use the static MSVC runtime (`/MT`). MinGW builds request static GCC and C++ runtimes. Always inspect the resulting DLL's dependencies with `dumpbin /dependents` or `objdump -p`; system API-set dependencies are expected.
