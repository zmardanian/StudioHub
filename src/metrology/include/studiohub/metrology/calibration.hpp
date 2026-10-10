#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace studiohub::metrology {

enum class CalibrationStatus { Valid, Expired, Unknown };

struct CalibrationPoint {
    double frequency_hz{};
    double correction_db{};
    double standard_uncertainty_db{};
};

struct CalibrationProfile {
    std::string instrument_id;
    std::string manufacturer;
    std::string model;
    std::string serial;
    std::string certificate_id;
    std::string calibration_date_utc;
    std::string expiry_date_utc;
    std::string laboratory;
    std::string traceability_statement;
    std::vector<CalibrationPoint> level_points;
    double fixed_gain_correction_db{0.0};
    double fixed_gain_uncertainty_db{0.0};

    [[nodiscard]] bool structurally_valid() const noexcept;
    [[nodiscard]] CalibrationStatus status_on(const std::string& utc_date) const;
    [[nodiscard]] std::optional<double> correction_db(double frequency_hz) const noexcept;
    [[nodiscard]] std::optional<double> standard_uncertainty_db(double frequency_hz) const noexcept;
};

struct CalibratedValue {
    double raw_db{};
    double corrected_db{};
    double standard_uncertainty_db{};
};

[[nodiscard]] std::optional<CalibratedValue>
apply_level_calibration(double raw_db, double frequency_hz,
                        const CalibrationProfile& profile,
                        const std::string& utc_date) noexcept;

} // namespace studiohub::metrology
