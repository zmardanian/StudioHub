#pragma once
#include <optional>
#include <string>
#include <vector>

namespace studiohub::metrology {

struct UncertaintyComponent {
    std::string name;
    double standard_uncertainty{};
    double sensitivity{1.0};
};

struct UncertaintyBudget {
    std::vector<UncertaintyComponent> components;
    double coverage_factor{2.0};
    [[nodiscard]] std::optional<double> combined_standard_uncertainty() const noexcept;
    [[nodiscard]] std::optional<double> expanded_uncertainty() const noexcept;
};

} // namespace studiohub::metrology
