#include "studiohub/metrology/calibration.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <array>

namespace studiohub::metrology {
namespace {
[[nodiscard]] bool valid_utc_date(const std::string& s) noexcept {
    if (s.size() != 10U || s[4] != '-' || s[7] != '-') return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == 4U || i == 7U) continue;
        if (std::isdigit(static_cast<unsigned char>(s[i])) == 0) return false;
    }
    const int year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    const int month = (s[5] - '0') * 10 + (s[6] - '0');
    const int day = (s[8] - '0') * 10 + (s[9] - '0');
    if (year < 1 || month < 1 || month > 12 || day < 1) return false;
    constexpr std::array<int, 12> days{{31,28,31,30,31,30,31,31,30,31,30,31}};
    int max_day = days[static_cast<std::size_t>(month - 1)];
    const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (month == 2 && leap) max_day = 29;
    return day <= max_day;
}
}

bool CalibrationProfile::structurally_valid() const noexcept {
    if (instrument_id.empty() || certificate_id.empty() || !valid_utc_date(calibration_date_utc) || laboratory.empty()) return false;
    if (!expiry_date_utc.empty() && !valid_utc_date(expiry_date_utc)) return false;
    if (!expiry_date_utc.empty() && calibration_date_utc > expiry_date_utc) return false;
    if (!std::isfinite(fixed_gain_correction_db) || !std::isfinite(fixed_gain_uncertainty_db) || fixed_gain_uncertainty_db < 0.0) return false;
    if (level_points.empty()) return true;
    for (std::size_t i = 0; i < level_points.size(); ++i) {
        const auto& p = level_points[i];
        if (!std::isfinite(p.frequency_hz) || p.frequency_hz <= 0.0 || !std::isfinite(p.correction_db) ||
            !std::isfinite(p.standard_uncertainty_db) || p.standard_uncertainty_db < 0.0) return false;
        if (i > 0 && level_points[i - 1].frequency_hz >= p.frequency_hz) return false;
    }
    return true;
}

CalibrationStatus CalibrationProfile::status_on(const std::string& utc_date) const {
    if (!valid_utc_date(utc_date) || !valid_utc_date(calibration_date_utc) || utc_date < calibration_date_utc) return CalibrationStatus::Unknown;
    if (!expiry_date_utc.empty() && (!valid_utc_date(expiry_date_utc) || utc_date > expiry_date_utc)) return CalibrationStatus::Expired;
    return CalibrationStatus::Valid;
}

std::optional<double> CalibrationProfile::correction_db(double frequency_hz) const noexcept {
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0 || !structurally_valid()) return std::nullopt;
    if (level_points.empty()) return fixed_gain_correction_db;
    if (frequency_hz < level_points.front().frequency_hz || frequency_hz > level_points.back().frequency_hz) return std::nullopt;
    const auto it = std::lower_bound(level_points.begin(), level_points.end(), frequency_hz,
        [](const CalibrationPoint& p, double f) { return p.frequency_hz < f; });
    if (it != level_points.end() && it->frequency_hz == frequency_hz) return fixed_gain_correction_db + it->correction_db;
    const auto hi = it;
    const auto lo = std::prev(it);
    const double t = (frequency_hz - lo->frequency_hz) / (hi->frequency_hz - lo->frequency_hz);
    return fixed_gain_correction_db + lo->correction_db + t * (hi->correction_db - lo->correction_db);
}

std::optional<double> CalibrationProfile::standard_uncertainty_db(double frequency_hz) const noexcept {
    if (!std::isfinite(frequency_hz) || frequency_hz <= 0.0 || !structurally_valid()) return std::nullopt;
    if (level_points.empty()) return fixed_gain_uncertainty_db;
    if (frequency_hz < level_points.front().frequency_hz || frequency_hz > level_points.back().frequency_hz) return std::nullopt;
    const auto it = std::lower_bound(level_points.begin(), level_points.end(), frequency_hz,
        [](const CalibrationPoint& p, double f) { return p.frequency_hz < f; });
    if (it != level_points.end() && it->frequency_hz == frequency_hz) return std::hypot(fixed_gain_uncertainty_db, it->standard_uncertainty_db);
    const auto hi = it;
    const auto lo = std::prev(it);
    // Do not interpolate uncertainty as though its frequency dependence were known.
    // Use the larger bracketing standard uncertainty as a conservative bound.
    const double u_curve = std::max(lo->standard_uncertainty_db, hi->standard_uncertainty_db);
    return std::hypot(fixed_gain_uncertainty_db, u_curve);
}

std::optional<CalibratedValue> apply_level_calibration(double raw_db, double frequency_hz,
                                                         const CalibrationProfile& profile,
                                                         const std::string& utc_date) noexcept {
    if (!std::isfinite(raw_db) || !valid_utc_date(utc_date)) return std::nullopt;
    if (profile.status_on(utc_date) != CalibrationStatus::Valid) return std::nullopt;
    const auto c = profile.correction_db(frequency_hz);
    const auto u = profile.standard_uncertainty_db(frequency_hz);
    if (!c || !u) return std::nullopt;
    return CalibratedValue{raw_db, raw_db + *c, *u};
}

} // namespace studiohub::metrology
