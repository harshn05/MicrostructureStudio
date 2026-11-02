
## Mathematical Model

### 1. Continuous Stochastic Nucleation

For a simulation area \(A\), nucleation rate \(\lambda\), and time interval \(\Delta t\), the number of newly nucleated grains follows a Poisson distribution:

$$
N_{\mathrm{new}} \sim \mathrm{Poisson}(\lambda A \Delta t)
$$

### 2. Isotropic Radial Growth

A nucleus born at position \((x_i, y_i)\) and time \(t_i\) grows radially at a constant velocity \(v_i\). Its radius at time \(t\) is:

$$
R_i(t) = v_i(t - t_i)
$$

### 3. Grain Arrival Time

The arrival time of grain \(i\) at a point \((x,y)\) is:

$$
\tau_i(x,y) =
t_i +
\frac{
    \sqrt{(x-x_i)^2 + (y-y_i)^2}
}{
    v_i
}
$$

### 4. Grain Ownership and Impingement

Each point in the simulation domain is assigned to the grain with the earliest arrival time:

$$
G(x,y) = \arg\min_i \tau_i(x,y)
$$

This determines grain ownership and naturally accounts for grain impingement.

### Model Characteristics

The resulting model is an **isotropic, constant-velocity, kinetic Voronoi-like growth model** with continuous stochastic nucleation and grain impingement.

It is important to distinguish this approach from:

- Curvature-driven grain-boundary migration.
- Phase-field models.
