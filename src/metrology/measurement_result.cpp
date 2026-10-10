#include "studiohub/metrology/measurement_result.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace studiohub::metrology {

namespace {
std::string esc(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) { case '"': out += "\\\""; break; case '\\': out += "\\\\"; break; case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break; case '\b': out += "\\b"; break; case '\f': out += "\\f"; break; default: if (static_cast<unsigned char>(c) < 0x20U) { const char* hex="0123456789abcdef"; out += "\\u00"; out += hex[(static_cast<unsigned char>(c) >> 4U) & 0x0FU]; out += hex[static_cast<unsigned char>(c) & 0x0FU]; } else { out += c; } }
    }
    return out;
}
}

bool MeasurementResult::valid() const noexcept {
    return !procedure_id.empty() && !software_version.empty() && !software_revision.empty() && !measurement_id.empty() &&
           !utc_timestamp.empty() && !instrument_id.empty() && !calibration_certificate.empty() &&
           !traceability_statement.empty() && !quantity.empty() && !unit.empty() && std::isfinite(value) &&
           uncertainty.combined_standard_uncertainty().has_value();
}

std::string MeasurementResult::to_json() const {
    std::ostringstream os;
    os << std::setprecision(17);
    os << "{\n"
       << "  \"procedure_id\":\"" << esc(procedure_id) << "\",\n"
       << "  \"software_version\":\"" << esc(software_version) << "\",\n"
       << "  \"software_revision\":\"" << esc(software_revision) << "\",\n"
       << "  \"measurement_id\":\"" << esc(measurement_id) << "\",\n"
       << "  \"utc_timestamp\":\"" << esc(utc_timestamp) << "\",\n"
       << "  \"instrument_id\":\"" << esc(instrument_id) << "\",\n"
       << "  \"calibration_certificate\":\"" << esc(calibration_certificate) << "\",\n"
       << "  \"traceability_statement\":\"" << esc(traceability_statement) << "\",\n"
       << "  \"quantity\":\"" << esc(quantity) << "\",\n"
       << "  \"unit\":\"" << esc(unit) << "\",\n"
       << "  \"value\":" << value << ",\n"
       << "  \"standard_uncertainty\":" << uncertainty.combined_standard_uncertainty().value() << ",\n"
       << "  \"expanded_uncertainty\":" << uncertainty.expanded_uncertainty().value() << ",\n"
       << "  \"coverage_factor\":" << uncertainty.coverage_factor << ",\n"
       << "  \"warnings\":[";
    for (std::size_t i = 0; i < warnings.size(); ++i) { if (i) os << ','; os << '"' << esc(warnings[i]) << '"'; }
    os << "]\n}\n";
    return os.str();
}

} // namespace studiohub::metrology
