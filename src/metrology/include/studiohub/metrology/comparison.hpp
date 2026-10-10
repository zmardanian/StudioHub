#pragma once
#include <optional>
#include <string>

namespace studiohub::metrology {

struct ComparisonPoint {
    std::string laboratory_id;
    double value{};
    double standard_uncertainty{};
};

struct InterlaboratoryComparison {
    ComparisonPoint participant;
    ComparisonPoint reference;
    double acceptance_limit_en{1.0};

    [[nodiscard]] std::optional<double> normalized_difference() const noexcept;
    [[nodiscard]] std::optional<bool> accepted() const noexcept;
};

} // namespace studiohub::metrology
