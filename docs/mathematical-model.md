# Mathematical model

## Continuous nucleation

Nucleation events are sampled from a spatially uniform Poisson process. For domain area \(A\), nucleation rate \(I\) (nuclei per pixel per second), and time increment \(\Delta t\):

\[
N_{\mathrm{new}} \sim \operatorname{Poisson}(I A \Delta t)
\]

Each new nucleus receives a uniformly sampled position in the 2D domain, a birth time uniformly sampled within the current step, a growth velocity sampled from the configured interval, and a display colour.

## Isotropic growth

For nucleus \(i\), with position \((x_i,y_i)\), birth time \(t_i\), and constant radial growth velocity \(v_i\), the radius is:

\[
R_i(t) = v_i(t-t_i), \qquad t \geq t_i
\]

The arrival time at a point \((x,y)\) is:

\[
\tau_i(x,y) = t_i +
\frac{\sqrt{(x-x_i)^2+(y-y_i)^2}}{v_i}
\]

The pixel is assigned to the grain with the smallest arrival time:

\[
G(x,y) = \underset{i}{\operatorname{argmin}}\;\tau_i(x,y)
\]

This is a kinetic, additively weighted Voronoi construction (often described as a Johnson-Mehl-type growth model for continuous nucleation).

## Numerical implementation

The domain is sampled at pixel centres. For each time step, the engine visits only the newly swept annulus between the previous and current radius of each grain. The stored arrival-time field ensures that the earliest arriving grain owns each pixel.

Grain boundaries in the current renderer are detected from changes in neighbouring pixel ownership.

## Scope and limitations

This is an isotropic, constant-velocity growth model with impingement. It is not a phase-field model and does not simulate curvature-driven grain-boundary migration, orientation-dependent mobility, anisotropic interfacial energy, or elastic effects. Pixel resolution and time-step size affect the discretized result.

The current engine advances on the UI timer and is intended as a compact interactive demonstrator. Large domains or high nucleation rates will benefit from spatial acceleration and moving simulation work off the UI thread.
