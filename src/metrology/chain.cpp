#include "studiohub/metrology/chain.hpp"
#include <cmath>
#include <sstream>
#include <array>
#include <cctype>

namespace studiohub::metrology {
namespace {
[[nodiscard]] bool nonempty(const std::string& s) noexcept { return !s.empty(); }

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

[[nodiscard]] bool date_ordered(const std::string& a, const std::string& b) noexcept {
    return a.empty() || b.empty() || a <= b;
}
[[nodiscard]] std::string esc(const std::string& s) {
    std::string out; out.reserve(s.size() + 8U);
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}
}

bool CertificateEvidence::structurally_valid() const noexcept {
    if (!nonempty(certificate_id) || !nonempty(provider) || !valid_utc_date(issue_date_utc) ||
        !nonempty(document_sha256) || (!expiry_date_utc.empty() && !valid_utc_date(expiry_date_utc)) ||
        !date_ordered(issue_date_utc, expiry_date_utc)) return false;
    const bool accreditation_partial =
        !accreditation_body.empty() || !accreditation_number.empty() || !scope_reference.empty();
    if (accreditation_partial &&
        (accreditation_body.empty() || accreditation_number.empty() || scope_reference.empty())) return false;
    if (document_sha256.size() != 64U) return false;
    for (const char c : document_sha256) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

EvidenceStatus CertificateEvidence::status_on(const std::string& utc_date) const noexcept {
    if (!structurally_valid() || !valid_utc_date(utc_date) || utc_date < issue_date_utc) return EvidenceStatus::Invalid;
    if (!expiry_date_utc.empty() && utc_date > expiry_date_utc) return EvidenceStatus::Expired;
    return status == EvidenceStatus::Valid || status == EvidenceStatus::Present ? EvidenceStatus::Valid : status;
}

bool InstrumentRecord::structurally_valid() const noexcept {
    return nonempty(instrument_id) && nonempty(manufacturer) && nonempty(model) &&
           nonempty(serial) && nonempty(role) && in_service;
}

TraceabilityStatus TraceabilityChain::assess() const noexcept {
    if (nodes.empty() || links.empty() || stated_reference.empty() || root_node_id.empty()) return TraceabilityStatus::Broken;
    bool root_exists = false;
    for (const auto& n : nodes) {
        if (n.node_id.empty() || n.reference.empty() || n.calibration_event_id.empty() || n.uncertainty_reference.empty()) return TraceabilityStatus::Broken;
        if (n.node_id == root_node_id) root_exists = true;
        for (const auto& other : nodes) {
            if (&other != &n && other.node_id == n.node_id) return TraceabilityStatus::Broken;
        }
    }
    if (!root_exists) return TraceabilityStatus::Broken;
    for (const auto& l : links) {
        if (l.from_node.empty() || l.to_node.empty() || l.evidence_id.empty() || l.from_node == l.to_node) return TraceabilityStatus::Broken;
        bool from_exists = false; bool to_exists = false;
        for (const auto& n : nodes) { from_exists = from_exists || n.node_id == l.from_node; to_exists = to_exists || n.node_id == l.to_node; }
        if (!from_exists || !to_exists) return TraceabilityStatus::Broken;
    }
    // Every node must be reachable from the declared reference root. This prevents disconnected
    // traceability islands from being accepted as one calibration hierarchy.
    std::vector<std::string> reachable{root_node_id};
    for (std::size_t pass = 0; pass < nodes.size(); ++pass) {
        bool changed = false;
        for (const auto& l : links) {
            bool from_reachable = false;
            for (const auto& id : reachable) from_reachable = from_reachable || id == l.from_node;
            bool already = false;
            for (const auto& id : reachable) already = already || id == l.to_node;
            if (from_reachable && !already) { reachable.push_back(l.to_node); changed = true; }
        }
        if (!changed) break;
    }
    if (reachable.size() != nodes.size()) return TraceabilityStatus::Broken;
    return TraceabilityStatus::Complete;
}

bool EnvironmentalObservation::valid() const noexcept {
    return !timestamp_utc.empty() && std::isfinite(temperature_c) && temperature_c > -100.0 && temperature_c < 150.0 &&
           std::isfinite(relative_humidity_percent) && relative_humidity_percent >= 0.0 && relative_humidity_percent <= 100.0 &&
           std::isfinite(pressure_hpa) && pressure_hpa > 300.0 && pressure_hpa < 1200.0;
}

bool MeasurementChain::structurally_valid() const noexcept {
    if (chain_id.empty() || instrument_ids.empty() || procedure_id.empty()) return false;
    for (const auto& id : instrument_ids) if (id.empty()) return false;
    for (const auto& e : environment) if (!e.valid()) return false;
    return true;
}

GoldenChainAssessment assess_golden_chain(const std::vector<InstrumentRecord>& instruments,
                                          const std::vector<CertificateEvidence>& certificates,
                                          const TraceabilityChain& traceability,
                                          const MeasurementChain& chain,
                                          const std::string& utc_date) noexcept {
    GoldenChainAssessment out;
    out.traceability = traceability.assess();
    if (out.traceability != TraceabilityStatus::Complete) out.blockers.emplace_back("metrological traceability chain is incomplete");
    if (out.traceability == TraceabilityStatus::Complete) {
        const TraceabilityNode* root = nullptr;
        for (const auto& node : traceability.nodes) {
            if (node.node_id == traceability.root_node_id) { root = &node; break; }
        }
        if (root == nullptr || root->reference != traceability.stated_reference) {
            out.blockers.emplace_back("declared traceability reference does not match the root node");
        }
        for (const auto& link : traceability.links) {
            bool evidence_found = false;
            for (const auto& certificate : certificates) {
                if (certificate.certificate_id == link.evidence_id) {
                    evidence_found = true;
                    if (certificate.status_on(utc_date) != EvidenceStatus::Valid) {
                        out.blockers.emplace_back("traceability evidence invalid or expired: " + certificate.certificate_id);
                    }
                    break;
                }
            }
            if (!evidence_found) out.blockers.emplace_back("traceability evidence missing: " + link.evidence_id);
        }
    }
    if (!chain.structurally_valid()) out.blockers.emplace_back("measurement chain metadata is incomplete or invalid");
    if (chain.environment.empty()) out.blockers.emplace_back("no environmental observation recorded");
    else if (!chain.environment.back().controlled) out.blockers.emplace_back("latest environment is not marked controlled");

    for (const auto& id : chain.instrument_ids) {
        const InstrumentRecord* found = nullptr;
        for (const auto& instrument : instruments) if (instrument.instrument_id == id) { found = &instrument; break; }
        if (found == nullptr || !found->structurally_valid()) {
            out.blockers.emplace_back("instrument missing or out of service: " + id);
            continue;
        }
        if (found->calibration_certificate_id.empty()) {
            out.blockers.emplace_back("instrument has no calibration certificate: " + id);
            continue;
        }
        const CertificateEvidence* cert = nullptr;
        for (const auto& c : certificates) if (c.certificate_id == found->calibration_certificate_id) { cert = &c; break; }
        if (cert == nullptr) out.blockers.emplace_back("calibration evidence missing: " + found->calibration_certificate_id);
        else {
            if (cert->status_on(utc_date) != EvidenceStatus::Valid) out.blockers.emplace_back("calibration evidence invalid or expired: " + cert->certificate_id);
            if (chain.requires_accredited_calibration && (cert->accreditation_body.empty() || cert->accreditation_number.empty() || cert->scope_reference.empty())) out.blockers.emplace_back("accreditation scope evidence missing: " + cert->certificate_id);
        }
    }
    out.ready_for_measurement = out.blockers.empty();
    return out;
}

std::string to_json(const GoldenChainAssessment& a) {
    std::ostringstream os;
    os << "{\n  \"ready_for_measurement\":" << (a.ready_for_measurement ? "true" : "false")
       << ",\n  \"traceability\":" << static_cast<int>(a.traceability) << ",\n  \"blockers\":[";
    for (std::size_t i = 0; i < a.blockers.size(); ++i) {
        if (i != 0U) os << ',';
        os << "\"" << esc(a.blockers[i]) << "\"";
    }
    os << "]\n}\n";
    return os.str();
}

} // namespace studiohub::metrology
