#include "studiohub/metrology/reference_signal.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace studiohub::metrology {

namespace {
[[nodiscard]] double db_to_linear(double db) noexcept { return std::pow(10.0, db / 20.0); }
}

bool valid(const ReferenceSignalSpec& s) noexcept {
    if (!std::isfinite(s.sample_rate_hz) || s.sample_rate_hz < 8000.0 || s.sample_rate_hz > 768000.0) return false;
    if (!std::isfinite(s.duration_seconds) || s.duration_seconds <= 0.0 || s.duration_seconds > 3600.0) return false;
    if (!std::isfinite(s.frequency_hz) || !std::isfinite(s.frequency2_hz) ||
        !std::isfinite(s.amplitude_dbfs) || !std::isfinite(s.amplitude2_dbfs) || !std::isfinite(s.phase_radians)) return false;
    if (s.amplitude_dbfs > 0.0 || s.amplitude2_dbfs > 0.0) return false;
    const double sample_count = std::ceil(s.sample_rate_hz * s.duration_seconds);
    if (!std::isfinite(sample_count) || sample_count <= 0.0 || sample_count > 10000000.0) return false;
    switch (s.type) {
    case ReferenceSignalType::Sine:
        return s.frequency_hz > 0.0 && s.frequency_hz < s.sample_rate_hz * 0.5;
    case ReferenceSignalType::TwoTone: {
        if (s.frequency_hz <= 0.0 || s.frequency2_hz <= 0.0 ||
            s.frequency_hz >= s.sample_rate_hz * 0.5 || s.frequency2_hz >= s.sample_rate_hz * 0.5) return false;
        const double peak_bound = db_to_linear(s.amplitude_dbfs) + db_to_linear(s.amplitude2_dbfs);
        return std::isfinite(peak_bound) && peak_bound <= 1.0;
    }
    case ReferenceSignalType::Impulse:
    case ReferenceSignalType::Silence:
        return true;
    }
    return false;
}

std::string stable_id(const ReferenceSignalSpec& s) {
    if (!valid(s)) return {};
    std::ostringstream os;
    os << "SHRS1-" << std::setprecision(17)
       << static_cast<int>(s.type) << '-' << s.sample_rate_hz << '-' << s.frequency_hz << '-'
       << s.frequency2_hz << '-' << s.amplitude_dbfs << '-' << s.amplitude2_dbfs << '-'
       << s.duration_seconds << '-' << s.phase_radians;
    return os.str();
}

ReferenceSignal generate(const ReferenceSignalSpec& s) {
    if (!valid(s)) throw std::invalid_argument("invalid reference signal specification");
    const double sample_count = std::ceil(s.sample_rate_hz * s.duration_seconds);
    if (!std::isfinite(sample_count) || sample_count <= 0.0 || sample_count > 10000000.0) throw std::invalid_argument("invalid reference signal sample count");
    const auto n = static_cast<std::size_t>(sample_count);
    ReferenceSignal out;
    out.samples.resize(n, 0.0);
    out.specification_id = stable_id(s);
    const double a1 = db_to_linear(s.amplitude_dbfs);
    const double a2 = db_to_linear(s.amplitude2_dbfs);
    const double w1 = 2.0 * std::numbers::pi * s.frequency_hz / s.sample_rate_hz;
    const double w2 = 2.0 * std::numbers::pi * s.frequency2_hz / s.sample_rate_hz;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i);
        switch (s.type) {
        case ReferenceSignalType::Sine: out.samples[i] = a1 * std::sin(s.phase_radians + w1 * t); break;
        case ReferenceSignalType::TwoTone: out.samples[i] = a1 * std::sin(s.phase_radians + w1 * t) + a2 * std::sin(s.phase_radians + w2 * t); break;
        case ReferenceSignalType::Impulse: if (i == 0U) out.samples[i] = a1; break;
        case ReferenceSignalType::Silence: break;
        }
    }
    return out;
}

} // namespace studiohub::metrology
