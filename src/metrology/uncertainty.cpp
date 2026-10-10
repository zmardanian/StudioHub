#include "studiohub/metrology/uncertainty.hpp"
#include <cmath>

namespace studiohub::metrology {

std::optional<double> UncertaintyBudget::combined_standard_uncertainty() const noexcept {
    if (!std::isfinite(coverage_factor) || coverage_factor <= 0.0) return std::nullopt;
    long double sum = 0.0L;
    for (const auto& c : components) {
        if (c.name.empty() || !std::isfinite(c.standard_uncertainty) || c.standard_uncertainty < 0.0 || !std::isfinite(c.sensitivity)) return std::nullopt;
        const long double term = static_cast<long double>(c.standard_uncertainty) * static_cast<long double>(c.sensitivity);
        sum += term * term;
    }
    return static_cast<double>(std::sqrt(sum));
}

std::optional<double> UncertaintyBudget::expanded_uncertainty() const noexcept {
    const auto u = combined_standard_uncertainty();
    if (!u) return std::nullopt;
    return *u * coverage_factor;
}

} // namespace studiohub::metrology
