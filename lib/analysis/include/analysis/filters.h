#pragma once

#include <givde/types.hpp>

using namespace givde;

namespace analysis {
// Perform convolution, using reflected boundary bonditions
f64 convolve(MatX<f64> const &input, MatX<f64> const & kernel, Eigen::Index row, Eigen::Index col);

// Calculates the frost filter, which is defined by:
// Output = (P_1 * K_1 + P_2 * K_2 + ... + P_n * K_n) / (K_1 + K_2 + ... + K_n)
//  Where K_i = exp(-B * S)
//  Where B = D * (L_v / L_M * L_M)
// D: damping factor, L_v the local variance, L_m is the local mean, S is the distance from the center pixel in the window, and P_i are the pixel values
// Values on the boundary are reflected
MatX<f64> frost_filter(MatX<f64> const &input_image, int kernel_size, f64 damping_factor=2.0);
}