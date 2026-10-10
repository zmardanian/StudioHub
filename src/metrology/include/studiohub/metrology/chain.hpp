#pragma once
#include <optional>
#include <string>
#include <vector>

namespace studiohub::metrology {

enum class EvidenceStatus { Missing, Present, Valid, Expired, Invalid };

enum class TraceabilityStatus { Unassessed, Complete, Broken, Unsupported };

struct CertificateEvidence {
    std::string certificate_id;
    std::string provider;
    std::string issue_date_utc;
    std::string expiry_date_utc;
    std::string document_sha256;
    std::string accreditation_body;
    std::string accreditation_number;
    std::string scope_reference;
    EvidenceStatus status{EvidenceStatus::Missing};

    [[nodiscard]] bool structurally_valid() const noexcept;
    [[nodiscard]] EvidenceStatus status_on(const std::string& utc_date) const noexcept;
};

struct InstrumentRecord {
    std::string instrument_id;
    std::string manufacturer;
    std::string model;
    std::string serial;
    std::string asset_tag;
    std::string role;
    std::string calibration_certificate_id;
    bool reference_standard{false};
    bool in_service{true};

    [[nodiscard]] bool structurally_valid() const noexcept;
};

struct TraceabilityNode {
    std::string node_id;
    std::string reference;
    std::string calibration_event_id;
    std::string uncertainty_reference;
};

struct TraceabilityLink {
    std::string from_node;
    std::string to_node;
    std::string evidence_id;
};

struct TraceabilityChain {
    std::vector<TraceabilityNode> nodes;
    std::vector<TraceabilityLink> links;
    std::string stated_reference;
    std::string root_node_id;

    [[nodiscard]] TraceabilityStatus assess() const noexcept;
};

struct EnvironmentalObservation {
    std::string timestamp_utc;
    double temperature_c{20.0};
    double relative_humidity_percent{50.0};
    double pressure_hpa{1013.25};
    bool controlled{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct MeasurementChain {
    std::string chain_id;
    std::vector<std::string> instrument_ids;
    std::string fixture_id;
    std::string procedure_id;
    bool requires_accredited_calibration{false};
    std::vector<EnvironmentalObservation> environment;

    [[nodiscard]] bool structurally_valid() const noexcept;
};

struct GoldenChainAssessment {
    bool ready_for_measurement{false};
    TraceabilityStatus traceability{TraceabilityStatus::Unassessed};
    std::vector<std::string> blockers;
};

[[nodiscard]] GoldenChainAssessment assess_golden_chain(
    const std::vector<InstrumentRecord>& instruments,
    const std::vector<CertificateEvidence>& certificates,
    const TraceabilityChain& traceability,
    const MeasurementChain& chain,
    const std::string& utc_date) noexcept;

[[nodiscard]] std::string to_json(const GoldenChainAssessment& assessment);

} // namespace studiohub::metrology
