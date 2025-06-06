// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file uccZdc.cxx
///
/// \brief task for analysis of UCC with the ZDC
/// \author Omar Vazquez (omar.vazquez.rueda@cern.ch)
/// \since January 29, 2025

#include <CCDB/BasicCCDBManager.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CCDB/EventSelectionParams.h"
#include "Common/CCDB/TriggerAliases.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "CommonConstants/MathConstants.h"
#include "CommonConstants/ZDCConstants.h"
#include "Framework/ASoAHelpers.h" // required for Filter op.
#include "Framework/AnalysisDataModel.h"
#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/O2DatabasePDGPlugin.h"
#include "Framework/runDataProcessing.h"
#include "ReconstructionDataFormats/GlobalTrackID.h"
#include "ReconstructionDataFormats/Track.h"
#include "TPDGCode.h"

using namespace std;
using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::aod::evsel;
using namespace o2::constants::physics;
using namespace o2::constants::math;

namespace o2::aod
{
using ColEvSels =
  soa::Join<aod::Collisions, aod::EvSels, aod::FT0MultZeqs,
            o2::aod::CentFT0Cs, aod::TPCMults, o2::aod::BarrelMults>;
using BCsRun3 =
  soa::Join<aod::BCsWithTimestamps, aod::BcSels, aod::Run3MatchedToBCSparse>;
using TracksSel = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCovIU,
                            aod::TrackSelection, aod::TracksDCA>;
using SimCollisions = soa::Join<aod::Collisions, aod::EvSels,
                                aod::McCollisionLabels, o2::aod::CentFT0Cs>;
using SimTracks = soa::Join<aod::Tracks, aod::TrackSelection, aod::TracksExtra,
                            aod::TracksDCA, aod::McTrackLabels>;
} // namespace o2::aod

struct UccZdc {
  // Event selection
  Configurable<float> posZcut{"posZcut", +10.0, "z-vertex position cut"};
  Configurable<float> minT0CcentCut{"minT0CcentCut", 0.0, "Min T0C Cent. cut"};
  Configurable<float> maxT0CcentCut{"maxT0CcentCut", 90.0, "Max T0C Cent. cut"};

  Configurable<float> minPt{"minPt", 0.1, "minimum pt of the tracks"};
  Configurable<float> maxPt{"maxPt", 50., "maximum pt of the tracks"};
  Configurable<float> minEta{"minEta", -0.8, "minimum eta"};
  Configurable<float> maxEta{"maxEta", +0.8, "maximum eta"};

  // Configurables, binning
  Configurable<int> nBinsAmpFV0{"nBinsAmpFV0", 100, "N bins FV0 amp"};
  Configurable<float> maxAmpFV0{"maxAmpFV0", 2000, "Max FV0 amp"};
  Configurable<int> nBinsAmpFT0{"nBinsAmpFT0", 100, "N bins FT0 amp"};
  Configurable<float> maxAmpFT0{"maxAmpFT0", 2500, "Max FT0 amp"};
  Configurable<int> nBinsNch{"nBinsNch", 2501, "N bins Nch (|eta|<0.8)"};
  Configurable<float> minNch{"minNch", 0, "Min Nch (|eta|<0.8)"};
  Configurable<float> maxNch{"maxNch", 2500, "Max Nch (|eta|<0.8)"};
  Configurable<int> nBinsZDC{"nBinsZDC", 400, "nBinsZDC"};
  Configurable<int> nBinsZEM{"nBinsZEM", 100, "nBinsZEM"};
  Configurable<float> maxZN{"maxZN", 150, "Max ZN signal"};
  Configurable<float> maxZP{"maxZP", 60, "Max ZP signal"};
  Configurable<float> maxZEM{"maxZEM", 2200, "Max ZEM signal"};
  Configurable<int> nBinsTDC{"nBinsTDC", 150, "nbinsTDC"};
  Configurable<float> minTdc{"minTdc", -15.0, "minimum TDC"};
  Configurable<float> maxTdc{"maxTdc", 15.0, "maximum TDC"};
  Configurable<float> minMeanpT{"minMeanpT", 0.5, "minimum [pT]"};
  Configurable<float> maxMeanpT{"maxMeanpT", 1.1, "maximum [pT]"};
  Configurable<int> nBinsMeanpT{"nBinsMeanpT", 160, "# bins [pT]"};
  ConfigurableAxis binsPt{"binsPt", {VARIABLE_WIDTH, 0.0, 0.1, 0.12}, "pT binning"};
  ConfigurableAxis binsCent{"binsCent", {VARIABLE_WIDTH, 0., 10., 20., 30., 40., 50., 60., 70., 80., 90., 100.}, "T0C binning"};

  // Configurable event selectiond and flags ZDC
  Configurable<bool> isNoCollInTimeRangeStrict{"isNoCollInTimeRangeStrict", true, "isNoCollInTimeRangeStrict?"};
  Configurable<bool> isNoCollInTimeRangeStandard{"isNoCollInTimeRangeStandard", false, "isNoCollInTimeRangeStandard?"};
  Configurable<bool> isNoCollInRofStrict{"isNoCollInRofStrict", true, "isNoCollInRofStrict?"};
  Configurable<bool> isNoCollInRofStandard{"isNoCollInRofStandard", false, "isNoCollInRofStandard?"};
  Configurable<bool> isNoHighMultCollInPrevRof{"isNoHighMultCollInPrevRof", true, "isNoHighMultCollInPrevRof?"};
  Configurable<bool> isNoCollInTimeRangeNarrow{"isNoCollInTimeRangeNarrow", false, "isNoCollInTimeRangeNarrow?"};
  Configurable<bool> isOccupancyCut{"isOccupancyCut", true, "Occupancy cut?"};
  Configurable<bool> isApplyFT0CbasedOccupancy{"isApplyFT0CbasedOccupancy", false, "T0C Occu cut?"};
  Configurable<bool> isAmpZDC{"isAmpZDC", false, "Use amplitude ZDC?"};
  Configurable<bool> isCommPMT{"isCommPMT", false, "Use common PMT ZDC?"};
  Configurable<bool> isSumTowers{"isSumTowers", false, "Use sum of Tow ZDC?"};
  Configurable<bool> isTDCcut{"isTDCcut", false, "Use TDC cut?"};
  Configurable<bool> isZEMcut{"isZEMcut", true, "Use ZEM cut?"};
  Configurable<bool> isZNbasedSel{"isZNbasedSel", false, "Use ZN based Sel."};
  Configurable<bool> isZNAbasedSel{"isZNAbasedSel", false, "Use ZNA based Sel."};
  Configurable<bool> isZNCbasedSel{"isZNCbasedSel", false, "Use ZNC based Sel."};
  Configurable<float> znBasedCut{"znBasedCut", 100, "ZN-based cut"};
  Configurable<float> zemCut{"zemCut", 1000., "ZEM cut"};
  Configurable<float> tdcCut{"tdcCut", 1., "TDC cut"};
  Configurable<float> minOccCut{"minOccCut", 0, "min Occu cut"};
  Configurable<float> maxOccCut{"maxOccCut", 500, "max Occu cut"};
  Configurable<int> minITSnCls{"minITSnCls", 5, "min ITSnCls"};

  enum EvCutLabel {
    All = 1,
    SelEigth,
    NoSameBunchPileup,
    IsGoodZvtxFT0vsPV,
    NoCollInTimeRangeStrict,
    NoCollInTimeRangeStandard,
    NoCollInRofStrict,
    NoCollInRofStandard,
    NoHighMultCollInPrevRof,
    NoCollInTimeRangeNarrow,
    OccuCut,
    Centrality,
    VtxZ,
    Zdc,
    TZero,
    Tdc,
    Zem
  };

  // Histograms names
  static constexpr std::string_view PsTitles[4] = {
    ";#it{N}_{ch} (|#eta| < 0.8, Corrected);#LT[#it{p}_{T}^{(1)}]#GT;",
    ";#it{N}_{ch} (|#eta| < 0.8, Corrected);#LT[#it{p}_{T}^{(2)}]#GT;",
    ";#it{N}_{ch} (|#eta| < 0.8, Corrected);#LT[#it{p}_{T}^{(3)}]#GT;",
    ";#it{N}_{ch} (|#eta| < 0.8, Corrected);#LT[#it{p}_{T}^{(4)}]#GT;"};

  // Filters
  // Filter trackFilter = ((aod::track::eta > minEta) && (aod::track::eta < maxEta) && (aod::track::pt > minPt) && (aod::track::pt < maxPt) && requireGlobalTrackInFilter());
  // Remove the GlobalTrack filter to count also ITS tracks
  Filter trackFilter = ((aod::track::eta > minEta) && (aod::track::eta < maxEta) && (aod::track::pt > minPt) && (aod::track::pt < maxPt));

  // Apply Filters
  // using TheFilteredCollisions = soa::Filtered<o2::aod::ColEvSels>;
  // using TheFilteredCollision = TheFilteredCollisions::iterator;
  using TheFilteredTracks = soa::Filtered<o2::aod::TracksSel>;
  // using TheFilteredTrack = TheFilteredTracks::iterator;

  // using TheFilteredSimCollisions = soa::Filtered<o2::aod::SimCollisions>;
  using TheFilteredSimTracks = soa::Filtered<o2::aod::SimTracks>;

  // Histograms: Data
  HistogramRegistry registry{
    "registry",
    {},
    OutputObjHandlingPolicy::AnalysisObject,
    true,
    true};

  Service<ccdb::BasicCCDBManager> ccdb;
  Configurable<std::string> paTH{"paTH", "Users/o/omvazque/TrackingEfficiency", "base path to the ccdb object"};
  Configurable<std::string> uRl{"uRl", "http://alice-ccdb.cern.ch", "url of the ccdb repository"};
  Configurable<int64_t> noLaterThan{"noLaterThan", 1740173636328, "latest acceptable timestamp of creation for the object"};

  // the efficiency has been previously stored in the CCDB as TH1F histogram
  TH1F* efficiency = nullptr;

  void init(InitContext const&)
  {
    // define axes you want to use
    const AxisSpec axisZpos{48, -12., 12., "Vtx_{z} (cm)"};
    const AxisSpec axisEvent{18, 0.5, 18.5, ""};
    const AxisSpec axisEta{30, -1.05, +1.05, "#eta"};
    const AxisSpec axisPt{binsPt, "#it{p}_{T} (GeV/#it{c})"};
    const AxisSpec axisDeltaPt{100, -1.0, +1.0, "#Delta(p_{T})"};
    const AxisSpec axisCent{binsCent, "T0C centrality"};
    const AxisSpec axisAmpCh{250, 0., 2500., "Amplitude of non-zero channels"};
    const AxisSpec axisEneCh{300, 0., 300., "Energy of non-zero channels"};

    registry.add("zPos", ";;Entries;", kTH1F, {axisZpos});
    registry.add("hEventCounter", ";;Events", kTH1F, {axisEvent});
    auto hstat = registry.get<TH1>(HIST("hEventCounter"));
    auto* x = hstat->GetXaxis();
    x->SetBinLabel(1, "All");
    x->SetBinLabel(2, "SelEigth");
    x->SetBinLabel(3, "NoSameBunchPileup");
    x->SetBinLabel(4, "GoodZvtxFT0vsPV");
    x->SetBinLabel(5, "NoCollInTimeRangeStrict");
    x->SetBinLabel(6, "NoCollInTimeRangeStandard");
    x->SetBinLabel(7, "NoCollInRofStrict");
    x->SetBinLabel(8, "NoCollInRofStandard");
    x->SetBinLabel(9, "NoHighMultCollInPrevRof");
    x->SetBinLabel(10, "NoCollInTimeRangeNarrow");
    x->SetBinLabel(11, "Occupancy Cut");
    x->SetBinLabel(12, "Cent. Sel.");
    x->SetBinLabel(13, "VtxZ cut");
    x->SetBinLabel(14, "has ZDC?");
    x->SetBinLabel(15, "has T0?");
    x->SetBinLabel(16, "Within TDC cut?");
    x->SetBinLabel(17, "Within ZEM cut?");

    //  Histograms: paritcle-level info
    if (doprocessZdcCollAss) {
      registry.add("NchVsZpos", ";#it{N}_{ch} (|#eta| < 0.8);Vtx_{z};",
                   kTProfile, {{nBinsNch, minNch, maxNch}});
      registry.add("ZposVsEta", "", kTProfile, {axisZpos});
      registry.add("EtaVsPhi", ";#eta;#varphi", kTH2F,
                   {{{axisEta}, {100, -0.1 * PI, +2.1 * PI}}});
      registry.add("NchVsPt", ";#it{N}_{ch} (|#eta| < 0.8, Corrected);;", kTH2F,
                   {{{nBinsNch, minNch, maxNch}, {axisPt}}});
      registry.add("sigma1Pt", ";;#sigma(p_{T})/p_{T};", kTProfile, {axisPt});
      registry.add("dcaXYvspT", ";DCA_{xy} (cm);;", kTH2F,
                   {{{50, -1., 1.}, {axisPt}}});
      registry.add("ZN", ";ZNA+ZNC;Entries;", kTH1F, {{nBinsZDC, -0.5, maxZN}});
      registry.add("ZNA", ";ZNA;Entries;", kTH1F, {{nBinsZDC, -0.5, maxZN}});
      registry.add("ZPA", ";ZPA;Entries;", kTH1F, {{nBinsZDC, -0.5, maxZP}});
      registry.add("ZNC", ";ZNC;Entries;", kTH1F, {{nBinsZDC, -0.5, maxZN}});
      registry.add("ZPC", ";ZPC;Entries;", kTH1F, {{nBinsZDC, -0.5, maxZP}});
      registry.add("ZNVsZEM", ";ZEM;ZNA+ZNC;", kTH2F,
                   {{{60, -0.5, maxZEM}, {60, -0.5, maxZN}}});
      registry.add("ZNAVsZNC", ";ZNC;ZNA", kTH2F,
                   {{{30, -0.5, maxZN}, {30, -0.5, maxZN}}});
      registry.add("ZPAVsZPC", ";ZPC;ZPA;", kTH2F,
                   {{{100, -0.5, maxZP}, {100, -0.5, maxZP}}});
      registry.add("ZNAVsZPA", ";ZPA;ZNA;", kTH2F,
                   {{{20, -0.5, maxZP}, {30, -0.5, maxZN}}});
      registry.add("ZNCVsZPC", ";ZPC;ZNC;", kTH2F,
                   {{{20, -0.5, maxZP}, {30, -0.5, maxZN}}});
      registry.add("ZNCVstdc", ";t_{ZNC};ZNC;", kTH2F,
                   {{{30, -15., 15.}, {nBinsZDC, -0.5, maxZN}}});
      registry.add("ZNAVstdc", ";t_{ZNA};ZNA;", kTH2F,
                   {{{30, -15., 15.}, {30, -0.5, maxZN}}});
      registry.add("ZPCVstdc", ";t_{ZPC};ZPC;", kTH2F,
                   {{{30, -15., 15}, {20, -0.5, maxZP}}});
      registry.add("ZPAVstdc", ";t_{ZPA};ZPA;", kTH2F,
                   {{{30, -15., 15.}, {20, -0.5, maxZP}}});
      registry.add("ZEM1Vstdc", ";t_{ZEM1};ZEM1;", kTH2F,
                   {{{30, -15., 15.}, {30, -0.5, 2000.5}}});
      registry.add("ZEM2Vstdc", ";t_{ZEM2};ZEM2;", kTH2F,
                   {{{30, -15., 15.}, {30, -0.5, 2000.5}}});
      registry.add("debunch", ";t_{ZDC}-t_{ZDA};t_{ZDC}+t_{ZDA}", kTH2F,
                   {{{nBinsTDC, minTdc, maxTdc}, {nBinsTDC, minTdc, maxTdc}}});
      registry.add("NchVsFT0C", ";T0C (#times 1/100);#it{N}_{ch} (|#eta|<0.8);", kTProfile, {{nBinsAmpFT0, 0., 950.}});
      registry.add("NchVsFT0M", ";T0A+T0C (#times 1/100);#it{N}_{ch} (|#eta|<0.8);", kTProfile, {{nBinsAmpFT0, 0., 3000.}});
      registry.add("NchVsFT0A", ";T0A (#times 1/100);#it{N}_{ch} (|#eta|<0.8);", kTProfile, {{nBinsAmpFT0, 0., maxAmpFT0}});
      registry.add("NchVsFV0A", ";V0A (#times 1/100);#it{N}_{ch} (|#eta|<0.8);", kTProfile, {{nBinsAmpFV0, 0., maxAmpFV0}});
      registry.add("NchVsNPV", ";#it{N}_{PV} (|#eta|<1);#LT ITS+TPC tracks #GT (|#eta|<0.8);", kTProfile, {{nBinsNch, minNch, maxNch}});
      registry.add("NchVsITStracks", ";ITS tracks nCls >= 5;#LTITS+TPC tracks#GT (|#eta|<0.8);", kTProfile, {{nBinsNch, minNch, maxNch}});
      registry.add("ZNCVsNch", ";#it{N}_{ch} (|#eta|<0.8);ZNC;", kTH2F,
                   {{{nBinsNch, minNch, maxNch}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNAVsNch", ";#it{N}_{ch} (|#eta|<0.8);ZNA;", kTH2F,
                   {{{nBinsNch, minNch, maxNch}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNVsNch", ";#it{N}_{ch} (|#eta|<0.8);ZNA+ZNC;", kTH2F,
                   {{{nBinsNch, minNch, maxNch}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNVsFT0A", ";T0A (#times 1/100);ZNA+ZNC;", kTH2F,
                   {{{nBinsAmpFT0, 0., maxAmpFT0}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNVsFT0C", ";T0C (#times 1/100);ZNA+ZNC;", kTH2F,
                   {{{nBinsAmpFT0, 0., maxAmpFT0}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNVsFT0M", ";T0A+T0C (#times 1/100);ZNA+ZNC;", kTH2F,
                   {{{nBinsAmpFT0, 0., 3000.}, {nBinsZDC, minNch, maxZN}}});
      registry.add("ZNDifVsNch", ";#it{N}_{ch} (|#eta|<0.8);ZNA-ZNC;", kTH2F,
                   {{{nBinsNch, minNch, maxNch}, {100, -50., 50.}}});
    }

    if (doprocessMCclosure || doprocessZdcCollAss) {
      registry.add("NchRaw", ";#it{N}_{ch} (|#eta| < 0.8);", kTH1F,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("Nch", ";#it{N}_{ch} (|#eta| < 0.8, Corrected);", kTH1F,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("T0Ccent", ";;Entries", kTH1F, {axisCent});
      registry.add("NchVsOneParCorr", PsTitles[0].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchVsTwoParCorr", PsTitles[1].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchVsThreeParCorr", PsTitles[2].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchVsFourParCorr", PsTitles[3].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
    }

    // MC Histograms
    if (doprocesspTEff) {
      registry.add("T0Ccent", ";;Entries", kTH1F, {axisCent});
      registry.add("nRecColvsCent", "", kTH2F, {{6, -0.5, 5.5}, {{axisCent}}});
      registry.add("Pt_all_ch", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_ch", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_pi", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_ka", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_pr", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_sigpos", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_signeg", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("Pt_re", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("EtaVsPhi", ";;#varphi;", kTH2F,
                   {{{axisEta}, {100, -0.1 * PI, +2.1 * PI}}});
      registry.add("hEventCounterMC", "Event counter", kTH1F, {axisEvent});
      registry.add("zPosMC", ";;Entries;", kTH1F, {axisZpos});
      registry.add("PtMC_ch", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_pi", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_ka", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_pr", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_sigpos", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_signeg", "", kTH2F, {{axisCent}, {axisPt}});
      registry.add("PtMC_re", "", kTH2F, {{axisCent}, {axisPt}});
    }

    if (doprocessMCclosure) {
      registry.add("NchGen", ";Nch;Entries;", kTH1F,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchvsOneParCorrGen", PsTitles[0].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchvsTwoParCorrGen", PsTitles[1].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchvsThreeParCorrGen", PsTitles[2].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
      registry.add("NchvsFourParCorrGen", PsTitles[3].data(), kTProfile,
                   {{nBinsNch, minNch, maxNch}});
    }

    ccdb->setURL(uRl.value);
    // Enabling object caching, otherwise each call goes to the CCDB server
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();
    // Not later than now, will be replaced by the value of the train creation
    // This avoids that users can replace objects **while** a train is running
    ccdb->setCreatedNotAfter(noLaterThan.value);
    LOGF(info, "Getting object %s", paTH.value.data());
    efficiency = ccdb->getForTimeStamp<TH1F>(paTH.value, noLaterThan);
    if (!efficiency) {
      LOGF(fatal, "Efficiency object not found!");
    }
  }

  template <typename CheckCol>
  bool isEventSelected(CheckCol const& col)
  {
    registry.fill(HIST("hEventCounter"), EvCutLabel::All);
    if (!col.sel8()) {
      return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::SelEigth);

    if (!col.selection_bit(o2::aod::evsel::kNoSameBunchPileup)) {
      return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::NoSameBunchPileup);

    if (!col.selection_bit(o2::aod::evsel::kIsGoodZvtxFT0vsPV)) {
      return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::IsGoodZvtxFT0vsPV);

    if (isNoCollInTimeRangeStrict) {
      if (!col.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStrict)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoCollInTimeRangeStrict);
    }

    if (isNoCollInTimeRangeStandard) {
      if (!col.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoCollInTimeRangeStandard);
    }

    if (isNoCollInRofStrict) {
      if (!col.selection_bit(o2::aod::evsel::kNoCollInRofStrict)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoCollInRofStrict);
    }

    if (isNoCollInRofStandard) {
      if (!col.selection_bit(o2::aod::evsel::kNoCollInRofStandard)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoCollInRofStandard);
    }

    if (isNoHighMultCollInPrevRof) {
      if (!col.selection_bit(o2::aod::evsel::kNoHighMultCollInPrevRof)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoHighMultCollInPrevRof);
    }

    // To be used in combination with FT0C-based occupancy
    if (isNoCollInTimeRangeNarrow) {
      if (!col.selection_bit(o2::aod::evsel::kNoCollInTimeRangeNarrow)) {
        return false;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::NoCollInTimeRangeNarrow);
    }

    if (isOccupancyCut) {
      auto occuValue{isApplyFT0CbasedOccupancy
                       ? col.ft0cOccupancyInTimeRange()
                       : col.trackOccupancyInTimeRange()};

      if (occuValue < minOccCut || occuValue > maxOccCut)
        return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::OccuCut);

    if (col.centFT0C() < minT0CcentCut || col.centFT0C() > maxT0CcentCut) {
      return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::Centrality);

    // Z-vertex position cut
    if (std::fabs(col.posZ()) > posZcut) {
      return false;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::VtxZ);

    return true;
  }

  void processZdcCollAss(o2::aod::ColEvSels::iterator const& collision,
                         o2::aod::BCsRun3 const& /*bcs*/,
                         aod::Zdcs const& /*zdcs*/, aod::FV0As const& /*fv0as*/,
                         aod::FT0s const& /*ft0s*/,
                         TheFilteredTracks const& tracks)
  {
    if (!isEventSelected(collision)) {
      return;
    }

    const auto& foundBC = collision.foundBC_as<o2::aod::BCsRun3>();
    if (!foundBC.has_zdc()) { // has ZDC?
      return;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::Zdc);

    float aT0A{0.0};
    float aT0C{0.0};
    float aV0A{0.0};
    float znA{0.0};
    float znC{0.0};
    float zpA{0.0};
    float zpC{0.0};
    float aZEM1{0.0};
    float aZEM2{0.0};
    float tZEM1{0.0};
    float tZEM2{0.0};
    float tZNA{0.0};
    float tZNC{0.0};
    float tZPA{0.0};
    float tZPC{0.0};
    float sumZNs{0.0};
    float sumZEMs{0.0};
    float tZDCdif{0.0};
    float tZDCsum{0.0};

    aZEM1 = foundBC.zdc().amplitudeZEM1();
    aZEM2 = foundBC.zdc().amplitudeZEM2();
    tZEM1 = foundBC.zdc().timeZEM1();
    tZEM2 = foundBC.zdc().timeZEM2();
    tZNA = foundBC.zdc().timeZNA();
    tZNC = foundBC.zdc().timeZNC();
    tZPA = foundBC.zdc().timeZPA();
    tZPC = foundBC.zdc().timeZPC();
    tZDCdif = tZNC + tZPC - tZNA - tZPA;
    tZDCsum = tZNC + tZPC + tZNA + tZPA;

    if (isAmpZDC) {
      znA = foundBC.zdc().amplitudeZNA();
      znC = foundBC.zdc().amplitudeZNC();
      zpA = foundBC.zdc().amplitudeZPA();
      zpC = foundBC.zdc().amplitudeZPC();
    } else if (isCommPMT) {
      znA = foundBC.zdc().energyCommonZNA();
      znC = foundBC.zdc().energyCommonZNC();
      zpA = foundBC.zdc().energyCommonZPA();
      zpC = foundBC.zdc().energyCommonZPC();
    } else if (isSumTowers) {
      for (const auto& eZNA : foundBC.zdc().energySectorZNA())
        znA += eZNA;
      for (const auto& eZNC : foundBC.zdc().energySectorZNC())
        znC += eZNC;
      for (const auto& eZPA : foundBC.zdc().energySectorZPA())
        zpA += eZPA;
      for (const auto& eZPC : foundBC.zdc().energySectorZPC())
        zpC += eZPC;
    } else {
      znA = -999.;
      znC = -999.;
      zpA = -999.;
      zpC = -999.;
    }
    znA /= 2.81;
    znC /= 2.81;
    zpA /= 2.81;
    zpC /= 2.81;
    sumZNs = znA + znC;
    sumZEMs = aZEM1 + aZEM2;

    if (foundBC.has_ft0()) {
      for (const auto& amplitude : foundBC.ft0().amplitudeA()) {
        aT0A += amplitude;
      }
      for (const auto& amplitude : foundBC.ft0().amplitudeC()) {
        aT0C += amplitude;
      }
    } else {
      return;
    }
    registry.fill(HIST("hEventCounter"), EvCutLabel::TZero);

    if (foundBC.has_fv0a()) {
      for (const auto& amplitude : foundBC.fv0a().amplitude()) {
        aV0A += amplitude;
      }
    } else {
      aV0A = -999.;
    }

    // TDC cut
    if (isTDCcut) {
      if (std::sqrt(std::pow(tZDCdif, 2.) + std::pow(tZDCsum, 2.)) > tdcCut) {
        return;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::Tdc);
    }

    // ZEM cut
    if (isZEMcut) {
      if (sumZEMs < zemCut) {
        return;
      }
      registry.fill(HIST("hEventCounter"), EvCutLabel::Zem);
    }

    registry.fill(HIST("zPos"), collision.posZ());
    registry.fill(HIST("T0Ccent"), collision.centFT0C());
    registry.fill(HIST("ZN"), znA + znC);
    registry.fill(HIST("ZNA"), znA);
    registry.fill(HIST("ZNC"), znC);
    registry.fill(HIST("ZPA"), zpA);
    registry.fill(HIST("ZPC"), zpC);
    registry.fill(HIST("ZNAVsZNC"), znC, znA);
    registry.fill(HIST("ZNAVsZPA"), zpA, znA);
    registry.fill(HIST("ZNCVsZPC"), zpC, znC);
    registry.fill(HIST("ZPAVsZPC"), zpC, zpA);
    registry.fill(HIST("ZNVsZEM"), sumZEMs, sumZNs);
    registry.fill(HIST("ZNCVstdc"), tZNC, znC);
    registry.fill(HIST("ZNAVstdc"), tZNA, znA);
    registry.fill(HIST("ZPCVstdc"), tZPC, zpC);
    registry.fill(HIST("ZPAVstdc"), tZPA, zpA);
    registry.fill(HIST("ZEM1Vstdc"), tZEM1, aZEM1);
    registry.fill(HIST("ZEM2Vstdc"), tZEM2, aZEM2);
    registry.fill(HIST("debunch"), tZDCdif, tZDCsum);

    std::vector<float> pTs;
    std::vector<float> wIs;
    float itsTracks{0.};

    // Calculates the event weight, W_k
    for (const auto& track : tracks) {
      if (track.hasITS() && track.itsNCls() >= minITSnCls) {
        itsTracks++;
      }
      // Track Selection
      if (!track.isGlobalTrack()) {
        continue;
      }

      registry.fill(HIST("EtaVsPhi"), track.eta(), track.phi());
      registry.fill(HIST("sigma1Pt"), track.pt(), track.sigma1Pt());
      registry.fill(HIST("dcaXYvspT"), track.dcaXY(), track.pt());

      float pt{track.pt()};
      double weight{efficiency->GetBinContent(efficiency->FindBin(pt))};
      if (weight > 0.) {
        pTs.emplace_back(pt);
        wIs.emplace_back(weight);
      }
    }

    double p1, p2, p3, p4, w1, w2, w3, w4;
    p1 = p2 = p3 = p4 = w1 = w2 = w3 = w4 = 0.0;
    getPTpowers(pTs, wIs, p1, w1, p2, w2, p3, w3, p4, w4);
    const double nch{static_cast<double>(pTs.size())};
    if (!(nch != 0.))
      return;

    registry.fill(HIST("NchVsFV0A"), aV0A / 100., nch);
    registry.fill(HIST("NchVsFT0A"), aT0A / 100., nch);
    registry.fill(HIST("NchVsFT0C"), aT0C / 100., nch);
    registry.fill(HIST("NchVsFT0M"), (aT0A + aT0C) / 100., nch);
    registry.fill(HIST("ZNVsFT0A"), aT0A / 100., sumZNs);
    registry.fill(HIST("ZNVsFT0C"), aT0C / 100., sumZNs);
    registry.fill(HIST("ZNVsFT0M"), (aT0A + aT0C) / 100., sumZNs);
    registry.fill(HIST("NchVsNPV"), collision.multNTracksPVeta1(), nch);
    registry.fill(HIST("NchVsITStracks"), itsTracks, nch);
    registry.fill(HIST("ZNAVsNch"), nch, znA);
    registry.fill(HIST("ZNCVsNch"), nch, znC);
    registry.fill(HIST("ZNVsNch"), nch, sumZNs);
    registry.fill(HIST("ZNDifVsNch"), nch, znA - znC);
    registry.fill(HIST("NchVsZpos"), nch, collision.posZ());

    // QA check
    for (const auto& track : tracks) {
      // Track Selection
      if (!track.isGlobalTrack()) {
        continue;
      }
      registry.fill(HIST("NchVsPt"), w1, track.pt());
      registry.fill(HIST("ZposVsEta"), collision.posZ(), track.eta());
    }

    // EbE one-particle pT correlation
    // P1 / W1
    double oneParCorr{p1 / w1};

    // EbE two-particle pT correlation
    // (P1^{2} - P2)/(W1^{2} - W2)
    double denTwoParCorr{std::pow(w1, 2.) - w2};
    double numTwoParCorr{std::pow(p1, 2.) - p2};
    double twoParCorr{numTwoParCorr / denTwoParCorr};

    // EbE three-particle pT correlation
    // (P1^{3} − 3P2P1 + 2P3)/(W1^{3} − 3W2W1 + 2W3)
    double denThreeParCorr{std::pow(w1, 3.) - 3. * w2 * w1 + 2. * w3};
    double numThreeParCorr{std::pow(p1, 3.) - 3. * p2 * p1 + 2. * p3};
    double threeParCorr{numThreeParCorr / denThreeParCorr};

    // EbE four-particle pT correlation
    // (P1^{4} − 6P2P1^{2} + 3P2^{2} + 8P3P1 − 6P4)/(W1^{4} − 6W2W1^{2} +
    // 3W2^{2} + 8W3W1 − 6W4)
    double denFourParCorr{std::pow(w1, 4.) - 6. * w2 * std::pow(w1, 2.) +
                          3. * std::pow(w2, 2.) + 8 * w3 * w1 - 6. * w4};
    double numFourParCorr{std::pow(p1, 4.) - 6. * p2 * std::pow(p1, 2.) +
                          3. * std::pow(p2, 2.) + 8 * p3 * p1 - 6. * p4};
    double fourParCorr{numFourParCorr / denFourParCorr};

    if (isZNbasedSel && (sumZNs > znBasedCut))
      return;
    if (isZNAbasedSel && (znA > znBasedCut))
      return;
    if (isZNCbasedSel && (znC > znBasedCut))
      return;

    registry.fill(HIST("Nch"), w1);
    registry.fill(HIST("NchRaw"), nch);
    registry.fill(HIST("NchVsOneParCorr"), w1, oneParCorr, w1);
    registry.fill(HIST("NchVsTwoParCorr"), w1, twoParCorr, denTwoParCorr);
    registry.fill(HIST("NchVsThreeParCorr"), w1, threeParCorr, denThreeParCorr);
    registry.fill(HIST("NchVsFourParCorr"), w1, fourParCorr, denFourParCorr);
  }
  PROCESS_SWITCH(UccZdc, processZdcCollAss, "Process ZDC W/Coll Ass.", true);

  void processMCclosure(
    aod::McCollisions::iterator const& mccollision,
    soa::SmallGroups<o2::aod::SimCollisions> const& collisions,
    aod::McParticles const& mcParticles,
    TheFilteredSimTracks const& simTracks)
  {
    //----- MC reconstructed -----//
    for (const auto& collision : collisions) {
      // Event selection
      if (!isEventSelected(collision))
        continue;
      // MC collision?
      if (!collision.has_mcCollision())
        continue;

      const auto& cent{collision.centFT0C()};
      registry.fill(HIST("T0Ccent"), cent);
      registry.fill(HIST("zPos"), collision.posZ());
      // registry.fill(HIST("nRecColvsCent"), collisions.size(), cent);

      const auto& groupedTracks =
        simTracks.sliceBy(perCollision, collision.globalIndex());

      std::vector<float> pTs;
      std::vector<float> wIs;

      // Calculates the event weight, W_k
      for (const auto& track : groupedTracks) {
        // Track Selection
        if (!track.isGlobalTrack())
          continue;

        // To match the primary particles
        if (!track.has_mcParticle())
          continue;
        const auto& particle = track.mcParticle();
        if (!particle.isPhysicalPrimary())
          continue;

        float pt{track.pt()};
        double trkEff{efficiency->GetBinContent(efficiency->FindBin(pt))};
        if (trkEff > 0.) {
          pTs.emplace_back(pt);
          wIs.emplace_back(1. / trkEff);
        }
      }

      const double nch{static_cast<double>(pTs.size())};
      if (!(nch != 0.))
        return;

      double p1, p2, p3, p4, w1, w2, w3, w4;
      p1 = p2 = p3 = p4 = w1 = w2 = w3 = w4 = 0.0;
      getPTpowers(pTs, wIs, p1, w1, p2, w2, p3, w3, p4, w4);

      const double denTwoParCorr{std::pow(w1, 2.) - w2};
      const double numTwoParCorr{std::pow(p1, 2.) - p2};
      const double denThreeParCorr{std::pow(w1, 3.) - 3. * w2 * w1 + 2. * w3};
      const double numThreeParCorr{std::pow(p1, 3.) - 3. * p2 * p1 + 2. * p3};
      const double denFourParCorr{
        std::pow(w1, 4.) - 6. * w2 * std::pow(w1, 2.) +
        3. * std::pow(w2, 2.) + 8 * w3 * w1 - 6. * w4};
      const double numFourParCorr{
        std::pow(p1, 4.) - 6. * p2 * std::pow(p1, 2.) +
        3. * std::pow(p2, 2.) + 8 * p3 * p1 - 6. * p4};

      const double oneParCorr{p1 / w1};
      const double twoParCorr{numTwoParCorr / denTwoParCorr};
      const double threeParCorr{numThreeParCorr / denThreeParCorr};
      const double fourParCorr{numFourParCorr / denFourParCorr};

      registry.fill(HIST("Nch"), w1);
      registry.fill(HIST("NchRaw"), nch);
      registry.fill(HIST("NchVsOneParCorr"), w1, oneParCorr, w1);
      registry.fill(HIST("NchVsTwoParCorr"), w1, twoParCorr, denTwoParCorr);
      registry.fill(HIST("NchVsThreeParCorr"), w1, threeParCorr,
                    denThreeParCorr);
      registry.fill(HIST("NchVsFourParCorr"), w1, fourParCorr, denFourParCorr);

      //--------------------------- Generated MC ---------------------------
      if (std::fabs(mccollision.posZ()) > posZcut)
        continue;

      std::vector<float> pTsMC;
      std::vector<float> wIsMC;

      // Calculates the event weight, W_k
      for (const auto& particle : mcParticles) {
        if (particle.eta() < minEta || particle.eta() > maxEta)
          continue;
        if (particle.pt() < minPt || particle.pt() > maxPt)
          continue;
        if (!particle.isPhysicalPrimary())
          continue;

        float pt{particle.pt()};
        pTsMC.emplace_back(pt);
        wIsMC.emplace_back(1.);
      }

      const double nchMC{static_cast<double>(pTsMC.size())};
      double p1MC, p2MC, p3MC, p4MC, w1MC, w2MC, w3MC, w4MC;
      p1MC = p2MC = p3MC = p4MC = w1MC = w2MC = w3MC = w4MC = 0.0;
      getPTpowers(pTsMC, wIsMC, p1MC, w1MC, p2MC, w2MC, p3MC, w3MC, p4MC, w4MC);

      const double denTwoParCorrMC{std::pow(w1MC, 2.) - w2MC};
      const double numTwoParCorrMC{std::pow(p1MC, 2.) - p2MC};
      const double denThreeParCorrMC{std::pow(w1MC, 3.) - 3. * w2MC * w1MC +
                                     2. * w3MC};
      const double numThreeParCorrMC{std::pow(p1MC, 3.) - 3. * p2MC * p1MC +
                                     2. * p3MC};
      const double denFourParCorrMC{
        std::pow(w1MC, 4.) - 6. * w2MC * std::pow(w1MC, 2.) +
        3. * std::pow(w2MC, 2.) + 8 * w3MC * w1MC - 6. * w4MC};
      const double numFourParCorrMC{
        std::pow(p1MC, 4.) - 6. * p2MC * std::pow(p1MC, 2.) +
        3. * std::pow(p2MC, 2.) + 8 * p3MC * p1MC - 6. * p4MC};

      const double oneParCorrMC{p1MC / w1MC};
      const double twoParCorrMC{numTwoParCorrMC / denTwoParCorrMC};
      const double threeParCorrMC{numThreeParCorrMC / denThreeParCorrMC};
      const double fourParCorrMC{numFourParCorrMC / denFourParCorrMC};

      registry.fill(HIST("NchGen"), nchMC);
      registry.fill(HIST("NchvsOneParCorrGen"), nchMC, oneParCorrMC, w1MC);
      registry.fill(HIST("NchvsTwoParCorrGen"), nchMC, twoParCorrMC,
                    denTwoParCorrMC);
      registry.fill(HIST("NchvsThreeParCorrGen"), nchMC, threeParCorrMC,
                    denThreeParCorrMC);
      registry.fill(HIST("NchvsFourParCorrGen"), nchMC, fourParCorrMC,
                    denFourParCorrMC);
    }
  }
  PROCESS_SWITCH(UccZdc, processMCclosure, "Process MC closure", false);

  Preslice<aod::McParticles> perMCCollision = aod::mcparticle::mcCollisionId;
  Preslice<TheFilteredSimTracks> perCollision = aod::track::collisionId;
  void processpTEff(aod::McCollisions::iterator const& mccollision,
                    soa::SmallGroups<o2::aod::SimCollisions> const& collisions,
                    aod::McParticles const& mcParticles,
                    TheFilteredSimTracks const& simTracks)
  {
    //----- MC reconstructed -----//
    for (const auto& collision : collisions) {
      // Event selection
      if (!isEventSelected(collision))
        continue;

      // MC collision?
      if (!collision.has_mcCollision())
        continue;

      registry.fill(HIST("zPos"), collision.posZ());
      registry.fill(HIST("nRecColvsCent"), collisions.size(),
                    collision.centFT0C());

      const auto& cent{collision.centFT0C()};
      registry.fill(HIST("T0Ccent"), cent);

      const auto& groupedTracks =
        simTracks.sliceBy(perCollision, collision.globalIndex());
      for (const auto& track : groupedTracks) {
        if (!track.has_mcParticle())
          continue;

        const auto& particle = track.mcParticle();
        registry.fill(HIST("Pt_all_ch"), cent, track.pt());
        registry.fill(HIST("EtaVsPhi"), track.eta(), track.phi());

        if (!particle.isPhysicalPrimary())
          continue;

        registry.fill(HIST("Pt_ch"), cent, track.pt());
        if (particle.pdgCode() == PDG_t::kPiPlus ||
            particle.pdgCode() == PDG_t::kPiMinus) {
          registry.fill(HIST("Pt_pi"), cent, track.pt());
        } else if (particle.pdgCode() == PDG_t::kKPlus ||
                   particle.pdgCode() == PDG_t::kKMinus) {
          registry.fill(HIST("Pt_ka"), cent, track.pt());
        } else if (particle.pdgCode() == PDG_t::kProton ||
                   particle.pdgCode() == PDG_t::kProtonBar) {
          registry.fill(HIST("Pt_pr"), cent, track.pt());
        } else if (particle.pdgCode() == PDG_t::kSigmaPlus ||
                   particle.pdgCode() == PDG_t::kSigmaBarMinus) {
          registry.fill(HIST("Pt_sigpos"), cent, track.pt());
        } else if (particle.pdgCode() == PDG_t::kSigmaMinus ||
                   particle.pdgCode() == PDG_t::kSigmaBarPlus) {
          registry.fill(HIST("Pt_signeg"), cent, track.pt());
        } else {
          registry.fill(HIST("Pt_re"), cent, track.pt());
        }
      }

      // Generated MC
      registry.fill(HIST("hEventCounterMC"), EvCutLabel::All);
      if (std::fabs(mccollision.posZ()) > posZcut)
        continue;
      registry.fill(HIST("zPosMC"), mccollision.posZ());
      registry.fill(HIST("hEventCounterMC"), EvCutLabel::VtxZ);

      for (const auto& particle : mcParticles) {
        if (particle.eta() < minEta || particle.eta() > maxEta) {
          continue;
        }
        if (particle.pt() < minPt || particle.pt() > maxPt) {
          continue;
        }
        if (!particle.isPhysicalPrimary()) {
          continue;
        }
        registry.fill(HIST("PtMC_ch"), cent, particle.pt());
        if (particle.pdgCode() == PDG_t::kPiPlus ||
            particle.pdgCode() == PDG_t::kPiMinus) { // pion
          registry.fill(HIST("PtMC_pi"), cent, particle.pt());
        } else if (particle.pdgCode() == PDG_t::kKPlus ||
                   particle.pdgCode() == PDG_t::kKMinus) { // kaon
          registry.fill(HIST("PtMC_ka"), cent, particle.pt());
        } else if (particle.pdgCode() == PDG_t::kProton ||
                   particle.pdgCode() == PDG_t::kProtonBar) { // proton
          registry.fill(HIST("PtMC_pr"), cent, particle.pt());
        } else if (particle.pdgCode() == PDG_t::kSigmaPlus ||
                   particle.pdgCode() ==
                     PDG_t::kSigmaBarMinus) { // positive sigma
          registry.fill(HIST("PtMC_sigpos"), cent, particle.pt());
        } else if (particle.pdgCode() == PDG_t::kSigmaMinus ||
                   particle.pdgCode() ==
                     PDG_t::kSigmaBarPlus) { // negative sigma
          registry.fill(HIST("PtMC_signeg"), cent, particle.pt());
        } else { // rest
          registry.fill(HIST("PtMC_re"), cent, particle.pt());
        }
      }
    }
  }
  PROCESS_SWITCH(UccZdc, processpTEff, "Process pT Eff", false);

  template <typename T, typename U>
  void getPTpowers(const T& pTs, const T& wIs, U& pOne, U& wOne, U& pTwo,
                   U& wTwo, U& pThree, U& wThree, U& pFour, U& wFour)
  {
    pOne = wOne = pTwo = wTwo = pThree = wThree = pFour = wFour = 0.;

    for (std::size_t i = 0; i < pTs.size(); ++i) {
      const float pTi{pTs.at(i)};
      const float wEighti{wIs.at(i)};
      pOne += wEighti * pTi;
      wOne += wEighti;
      pTwo += std::pow(wEighti * pTi, 2.);
      wTwo += std::pow(wEighti, 2.);
      pThree += std::pow(wEighti * pTi, 3.);
      wThree += std::pow(wEighti, 3.);
      pFour += std::pow(wEighti * pTi, 4.);
      wFour += std::pow(wEighti, 4.);
    }
  }

  // Single-Track Selection
  // template <typename T2>
  // bool passedTrackSelection(const T2& track) {
  //   if (track.eta() < minEta || track.eta() > maxEta) return false;
  //   if (track.pt() < minPt) return false;
  //
  //   if (!track.hasITS()) return false;
  //   if (track.itsNCls() < minItsNclusters) return false;
  //   if (!track.hasTPC()) return false;
  //   if (track.tpcNClsFound() < minTpcNclusters) return false;
  //   if (track.tpcNClsCrossedRows() < minTpcNcrossedRows) return false;
  //   if (track.tpcChi2NCl() > maxChiSquareTpc) return false;
  //   if (track.itsChi2NCl() > maxChiSquareIts) return false;
  //   // pt-dependent selection
  //   if (setDCAselectionPtDep) {
  //     if (std::fabs(track.dcaXY()) > (par0 + par1 / track.pt())) return
  //     false; if (std::fabs(track.dcaZ()) > (par0 + par1 / track.pt()))
  //     return false;
  //   }
  //   // standard selection
  //   if (!setDCAselectionPtDep) {
  //     if (std::fabs(track.dcaXY()) > maxDcaxy) return false;
  //     if (std::fabs(track.dcaZ()) > maxDcaz) return false;
  //   }
  //   return true;
  // }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<UccZdc>(cfgc)};
}
