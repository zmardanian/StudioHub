#include "studiohub/metrology/comparison.hpp"
#include <cmath>

namespace studiohub::metrology {

std::optional<double> InterlaboratoryComparison::normalized_difference() const noexcept {
    if (participant.laboratory_id.empty() || reference.laboratory_id.empty() ||
        !std::isfinite(participant.value) || !std::isfinite(reference.value) ||
        !std::isfinite(participant.standard_uncertainty) || participant.standard_uncertainty < 0.0 ||
        !std::isfinite(reference.standard_uncertainty) || reference.standard_uncertainty < 0.0 ||
        (participant.standard_uncertainty == 0.0 && reference.standard_uncertainty == 0.0)) return std::nullopt;
    return (participant.value - reference.value) /
           std::sqrt(participant.standard_uncertainty * participant.standard_uncertainty +
                     reference.standard_uncertainty * reference.standard_uncertainty);
}

std::optional<bool> InterlaboratoryComparison::accepted() const noexcept {
    if (!std::isfinite(acceptance_limit_en) || acceptance_limit_en <= 0.0) return std::nullopt;
    const auto en = normalized_difference();
    if (!en) return std::nullopt;
    return std::abs(*en) <= acceptance_limit_en;
}

} // namespace studiohub::metrology
