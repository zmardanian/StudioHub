#pragma once
#include "studiohub/metrology/uncertainty.hpp"
#include <string>
#include <vector>

namespace studiohub::metrology {

struct MeasurementResult {
    std::string procedure_id;
    std::string software_version;
    std::string software_revision;
    std::string measurement_id;
    std::string utc_timestamp;
    std::string instrument_id;
    std::string calibration_certificate;
    std::string traceability_statement;
    std::string unit;
    std::string quantity;
    double value{};
    UncertaintyBudget uncertainty;
    std::vector<std::string> warnings;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::string to_json() const;
};

} // namespace studiohub::metrology
