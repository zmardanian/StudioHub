#include <gtest/gtest.h>
#include "studiohub/metrology/calibration.hpp"
#include "studiohub/metrology/chain.hpp"
#include "studiohub/metrology/comparison.hpp"
#include "studiohub/metrology/measurement_result.hpp"
#include "studiohub/metrology/reference_signal.hpp"
#include "studiohub/metrology/uncertainty.hpp"
#include <cmath>
#include <stdexcept>
#include <limits>
#include <string>

using namespace studiohub::metrology;

TEST(MetrologyCalibration, ValidatesAndFailsClosed) {
    CalibrationProfile p;
    p.instrument_id="IFACE-001"; p.certificate_id="CAL-001"; p.calibration_date_utc="2026-01-01";
    p.expiry_date_utc="2027-01-01"; p.laboratory="Reference Lab";
    p.level_points={{100.0,0.10,0.02},{1000.0,0.20,0.03},{10000.0,0.40,0.05}};
    ASSERT_TRUE(p.structurally_valid());
    EXPECT_EQ(p.status_on("2026-09-30"), CalibrationStatus::Valid);
    EXPECT_NEAR(p.correction_db(550.0).value(),0.15,1e-12);
    EXPECT_FALSE(p.correction_db(20.0).has_value());
    EXPECT_TRUE(apply_level_calibration(-20.0,1000.0,p,"2026-09-30").has_value());
    EXPECT_FALSE(apply_level_calibration(-20.0,1000.0,p,"2027-01-02").has_value());
    EXPECT_FALSE(apply_level_calibration(-20.0,1000.0,p,"09-30-2026").has_value());
    EXPECT_EQ(p.status_on("2026-02-30"), CalibrationStatus::Unknown);
}

TEST(MetrologyUncertainty, UsesRssAndConservativeCalibrationBounds) {
    UncertaintyBudget u{{{"noise",0.1,1.0},{"cal",0.2,1.0}},2.0};
    EXPECT_NEAR(u.combined_standard_uncertainty().value(),std::sqrt(0.05),1e-12);
    EXPECT_NEAR(u.expanded_uncertainty().value(),2.0*std::sqrt(0.05),1e-12);
    UncertaintyBudget malformed{{{"",0.1,1.0}},2.0};
    EXPECT_FALSE(malformed.combined_standard_uncertainty().has_value());
}

TEST(MetrologyReferenceSignal, IsDeterministicAndBounded) {
    ReferenceSignalSpec s; s.type=ReferenceSignalType::Sine; s.sample_rate_hz=48000.0; s.frequency_hz=997.0; s.amplitude_dbfs=-20.0; s.duration_seconds=0.1;
    ASSERT_TRUE(valid(s));
    auto r=generate(s);
    EXPECT_EQ(r.samples.size(),4800U);
    EXPECT_EQ(r.specification_id,stable_id(s));
    double peak=0.0; for(double x:r.samples) peak=std::max(peak,std::abs(x));
    EXPECT_NEAR(20.0*std::log10(peak),-20.0,0.02);
    auto bad=s; bad.amplitude_dbfs=1.0; EXPECT_FALSE(valid(bad));
    auto nan_frequency=s; nan_frequency.frequency_hz=std::numeric_limits<double>::quiet_NaN(); EXPECT_FALSE(valid(nan_frequency));
    auto invalid_type=s; invalid_type.type=static_cast<ReferenceSignalType>(99); EXPECT_FALSE(valid(invalid_type));
    auto huge=s; huge.duration_seconds=1000.0; EXPECT_FALSE(valid(huge));
}

TEST(MetrologyTraceability, RequiresConnectedRootAndEvidence) {
    CertificateEvidence cert;
    cert.certificate_id="CERT-1"; cert.provider="Accredited Lab"; cert.issue_date_utc="2026-01-01"; cert.expiry_date_utc="2027-01-01";
    cert.document_sha256=std::string(64, 'a'); cert.accreditation_body="DAkkS"; cert.accreditation_number="D-K-TEST"; cert.scope_reference="audio-electrical-level"; cert.status=EvidenceStatus::Valid;
    ASSERT_TRUE(cert.structurally_valid());
    InstrumentRecord dac{"DAC-001","ReferenceMaker","REF-DAC","SN001","ASSET1","reference DAC","CERT-1",true,true};
    TraceabilityChain tc; tc.stated_reference="SI-derived electrical reference"; tc.root_node_id="N1";
    tc.nodes={{"N1","national/reference standard","CAL-EVENT-1","U1"},{"N2","reference DAC","CAL-EVENT-2","U2"}};
    tc.links={{"N1","N2","CERT-1"}};
    EXPECT_EQ(tc.assess(),TraceabilityStatus::Complete);
    tc.nodes.push_back({"N3","disconnected","CAL-EVENT-3","U3"});
    EXPECT_EQ(tc.assess(),TraceabilityStatus::Broken);
    tc.nodes.pop_back(); tc.links.clear(); EXPECT_EQ(tc.assess(),TraceabilityStatus::Broken);
    MeasurementChain chain; chain.chain_id="GOLDEN"; chain.instrument_ids={"DAC-001"}; chain.fixture_id="FIX"; chain.procedure_id="PROC"; chain.requires_accredited_calibration=true;
    chain.environment.push_back({"2026-09-30T07:00:00Z",22.0,45.0,1012.0,true});
    auto a=assess_golden_chain({dac},{cert},TraceabilityChain{{{"N1","ref","E1","U1"},{"N2","dac","E2","U2"}},{{"N1","N2","CERT-1"}},"ref","N1"},chain,"2026-09-30");
    EXPECT_TRUE(a.ready_for_measurement);
    auto unscoped=cert; unscoped.scope_reference.clear();
    auto b=assess_golden_chain({dac},{unscoped},TraceabilityChain{{{"N1","ref","E1","U1"},{"N2","dac","E2","U2"}},{{"N1","N2","CERT-1"}},"ref","N1"},chain,"2026-09-30");
    EXPECT_FALSE(b.ready_for_measurement);
    auto missing_evidence = TraceabilityChain{{{"N1","SI-derived electrical reference","E1","U1"},{"N2","reference DAC","E2","U2"}},{{"N1","N2","MISSING-CERT"}},"SI-derived electrical reference","N1"};
    auto c=assess_golden_chain({dac},{cert},missing_evidence,chain,"2026-09-30");
    EXPECT_FALSE(c.ready_for_measurement);
}

TEST(MetrologyComparison, ComputesNormalizedDifference) {
    InterlaboratoryComparison c{{"StudioHub",10.02,0.03},{"ReferenceLab",10.00,0.04},1.0};
    EXPECT_NEAR(c.normalized_difference().value(),0.4,1e-12);
    EXPECT_TRUE(c.accepted().value());
}

TEST(MetrologyMeasurementResult, ProducesValidJsonRecord) {
    UncertaintyBudget u{{{"noise",0.1,1.0}},2.0};
    MeasurementResult r; r.procedure_id="PROC"; r.software_version="1.0.3"; r.software_revision="rev"; r.measurement_id="M1"; r.utc_timestamp="2026-09-30T00:00:00Z"; r.instrument_id="DAC-001"; r.calibration_certificate="CERT-1"; r.traceability_statement="SI-derived electrical reference"; r.quantity="Level"; r.unit="dBFS"; r.value=-19.8; r.uncertainty=u;
    ASSERT_TRUE(r.valid());
    EXPECT_NE(r.to_json().find("expanded_uncertainty"),std::string::npos);
    r.calibration_certificate.clear();
    EXPECT_FALSE(r.valid());
}

TEST(MetrologyTraceability, ExpiredLinkEvidenceBlocksMeasurement) {
    CertificateEvidence cert;
    cert.certificate_id="CERT-OLD"; cert.provider="Lab"; cert.issue_date_utc="2020-01-01"; cert.expiry_date_utc="2021-01-01";
    cert.document_sha256=std::string(64, 'b'); cert.status=EvidenceStatus::Valid;
    ASSERT_TRUE(cert.structurally_valid());
    TraceabilityChain tc{{{"N1","reference","E1","U1"},{"N2","instrument","E2","U2"}},{{"N1","N2","CERT-OLD"}},"reference","N1"};
    MeasurementChain mc; mc.chain_id="C"; mc.instrument_ids={"I1"}; mc.fixture_id="F"; mc.procedure_id="P"; mc.environment.push_back({"2026-09-30T07:00:00Z",22.0,45.0,1012.0,true});
    InstrumentRecord inst{"I1","M","Model","S","A","measurement","CERT-OLD",false,true};
    auto a=assess_golden_chain({inst},{cert},tc,mc,"2026-09-30");
    EXPECT_FALSE(a.ready_for_measurement);
}
