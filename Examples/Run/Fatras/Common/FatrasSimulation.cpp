// This file is part of the Acts project.
//
// Copyright (C) 2019-2020 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Acts/Geometry/GeometryIdentifier.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/MagneticField/ConstantBField.hpp"
#include "Acts/MagneticField/InterpolatedBFieldMap.hpp"
#include "Acts/MagneticField/SharedBField.hpp"
#include "Acts/Propagator/EigenStepper.hpp"
#include "Acts/Propagator/Navigator.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Propagator/StraightLineStepper.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "ActsExamples/EventData/SimHit.hpp"
#include "ActsExamples/EventData/SimParticle.hpp"
#include "ActsExamples/Fatras/FatrasAlgorithm.hpp"
#include "ActsExamples/Fatras/FatrasOptions.hpp"
#include "ActsExamples/Framework/RandomNumbers.hpp"
#include "ActsExamples/Framework/Sequencer.hpp"
#include "ActsExamples/Io/Csv/CsvParticleWriter.hpp"
#include "ActsExamples/Io/Csv/CsvSimHitReader.hpp"
#include "ActsExamples/Io/Csv/CsvSimHitWriter.hpp"
#include "ActsExamples/Io/Root/RootParticleWriter.hpp"
#include "ActsExamples/Io/Root/RootSimHitWriter.hpp"
#include "ActsExamples/Options/CommonOptions.hpp"
#include "ActsExamples/Plugins/BField/BFieldOptions.hpp"
#include "ActsExamples/Plugins/BField/ScalableBField.hpp"
#include "ActsExamples/Utilities/Paths.hpp"
#include "ActsFatras/Kernel/PhysicsList.hpp"
#include "ActsFatras/Kernel/PointLikePhysicsList.hpp"
#include "ActsFatras/Kernel/Process.hpp"
#include "ActsFatras/Kernel/Simulator.hpp"
#include "ActsFatras/Physics/Decay/NoDecay.hpp"
#include "ActsFatras/Physics/EnergyLoss/BetheBloch.hpp"
#include "ActsFatras/Physics/EnergyLoss/BetheHeitler.hpp"
#include "ActsFatras/Physics/Scattering/Highland.hpp"
#include "ActsFatras/Selectors/ChargeSelectors.hpp"
#include "ActsFatras/Selectors/KinematicCasts.hpp"
#include "ActsFatras/Selectors/SelectorHelpers.hpp"
#include "ActsFatras/Selectors/SurfaceSelectors.hpp"

#include <boost/program_options.hpp>

#include "FatrasInternal.hpp"

namespace {

/// Simple struct to select surfaces where hits should be generated.
struct HitSurfaceSelector {
  bool sensitive = false;
  bool material = false;
  bool passive = false;

  /// Check if the surface should be used.
  bool operator()(const Acts::Surface& surface) const {
    if (sensitive and surface.associatedDetectorElement()) {
      return true;
    }
    if (material and surface.surfaceMaterial()) {
      return true;
    }
    if (passive) {
      return true;
    }
    return false;
  }
};

/// Setup Fatras simulation algorithms
///
/// @tparam magnetic_field_t Concrete magnetic field type
///
/// @param variables The configuration variables
/// @param sequencer The framework sequencer
/// @param randomNumbers The random number service
/// @param trackingGeometry The TrackingGeometry for the tracking setup
template <typename magnetic_field_t>
void setupSimulationAlgorithms(
    const ActsExamples::Options::Variables& vars,
    ActsExamples::Sequencer& sequencer,
    std::shared_ptr<const ActsExamples::RandomNumbers> randomNumbers,
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry,
    magnetic_field_t&& magneticField) {
  using namespace ActsExamples;

  // Read the log level
  auto logLevel = Options::readLogLevel(vars);

  // setup propagator-related types
  // use the default navigation
  using Navigator = Acts::Navigator;
  // propagate charged particles numerically in the given magnetic field
  using ChargedStepper = Acts::EigenStepper<magnetic_field_t>;
  using ChargedPropagator = Acts::Propagator<ChargedStepper, Navigator>;
  // propagate neutral particles with just straight lines
  using NeutralStepper = Acts::StraightLineStepper;
  using NeutralPropagator = Acts::Propagator<NeutralStepper, Navigator>;

  // setup simulator-related types
  using MinP = ActsFatras::Min<ActsFatras::Casts::P>;
  // charged particles w/ standard em physics list and selectable hits
  using ChargedSelector =
      ActsFatras::CombineAnd<ActsFatras::ChargedSelector, MinP>;
  using ChargedSimulator = ActsFatras::ParticleSimulator<
      ChargedPropagator, ActsFatras::ChargedElectroMagneticPhysicsList,
      ActsFatras::PointLikePhysicsList<>, HitSurfaceSelector,
      ActsFatras::NoDecay>;
  // neutral particles w/o physics and no hits
  using NeutralSelector =
      ActsFatras::CombineAnd<ActsFatras::NeutralSelector, MinP>;
  using NeutralSimulator =
      ActsFatras::ParticleSimulator<NeutralPropagator,
                                    ActsFatras::PhysicsList<>,
                                    ActsFatras::PointLikePhysicsList<>,
                                    ActsFatras::NoSurface, ActsFatras::NoDecay>;
  // full simulator type for charged and neutrals
  using Simulator = ActsFatras::Simulator<ChargedSelector, ChargedSimulator,
                                          NeutralSelector, NeutralSimulator>;
  // final algorihm type
  using SimulationAlgorithm = ActsExamples::FatrasAlgorithm<Simulator>;

  // construct the simulator
  Navigator navigator(trackingGeometry);
  // construct the charged simulator
  ChargedStepper chargedStepper(std::move(magneticField));
  ChargedPropagator chargedPropagator(std::move(chargedStepper), navigator);
  ChargedSimulator chargedSimulator(std::move(chargedPropagator), logLevel);
  // construct the neutral simulator
  NeutralStepper neutralStepper;
  NeutralPropagator neutralPropagator(std::move(neutralStepper), navigator);
  NeutralSimulator neutralSimulator(std::move(neutralPropagator), logLevel);
  // construct the combined simulator
  Simulator simulator(std::move(chargedSimulator), std::move(neutralSimulator));

  // construct/add the simulation algorithm
  auto fatras =
      ActsExamples::Options::readFatrasConfig(vars, std::move(simulator));
  fatras.inputParticles = kFatrasCollectionParticles;
  fatras.outputParticlesInitial = "particles_initial";
  fatras.outputParticlesFinal = "particles_final";
  fatras.outputSimHits = kFatrasCollectionHits;
  fatras.randomNumbers = randomNumbers;
  sequencer.addAlgorithm(
      std::make_shared<SimulationAlgorithm>(fatras, logLevel));

  // Output directory
  auto outputDir = vars["output-dir"].template as<std::string>();

  // Write simulation information as CSV files
  if (vars["output-csv"].template as<bool>()) {
    // write initial simulated particles
    ActsExamples::CsvParticleWriter::Config writeInitial;
    writeInitial.inputParticles = fatras.outputParticlesInitial;
    writeInitial.outputDir = outputDir;
    writeInitial.outputStem = fatras.outputParticlesInitial;
    sequencer.addWriter(std::make_shared<ActsExamples::CsvParticleWriter>(
        writeInitial, logLevel));

    // write simulated hits collection
    ActsExamples::CsvSimHitWriter::Config writeSimHits;
    writeSimHits.inputSimHits = fatras.outputSimHits;
    writeSimHits.outputDir = outputDir;
    writeSimHits.outputStem = "sim" + fatras.outputSimHits;
    sequencer.addWriter(std::make_shared<ActsExamples::CsvSimHitWriter>(
        writeSimHits, logLevel));

    // write final simulated particles
    ActsExamples::CsvParticleWriter::Config writeFinal;
    writeFinal.inputParticles = fatras.outputParticlesFinal;
    writeFinal.outputDir = outputDir;
    writeFinal.outputStem = fatras.outputParticlesFinal;
    sequencer.addWriter(std::make_shared<ActsExamples::CsvParticleWriter>(
        writeFinal, logLevel));
  }

  // Write simulation information as ROOT files
  if (vars["output-root"].template as<bool>()) {
    // write initial simulated particles
    ActsExamples::RootParticleWriter::Config writeInitial;
    writeInitial.inputParticles = fatras.outputParticlesInitial;
    writeInitial.filePath = ActsExamples::joinPaths(
        outputDir, fatras.outputParticlesInitial + ".root");
    sequencer.addWriter(std::make_shared<ActsExamples::RootParticleWriter>(
        writeInitial, logLevel));

    // write final simulated particles
    ActsExamples::RootParticleWriter::Config writeFinal;
    writeFinal.inputParticles = fatras.outputParticlesFinal;
    writeFinal.filePath = ActsExamples::joinPaths(
        outputDir, fatras.outputParticlesFinal + ".root");
    sequencer.addWriter(std::make_shared<ActsExamples::RootParticleWriter>(
        writeFinal, logLevel));

    // write simulated hits
    ActsExamples::RootSimHitWriter::Config writeHits;
    writeHits.inputSimHits = fatras.outputSimHits;
    writeHits.filePath =
        ActsExamples::joinPaths(outputDir, fatras.outputSimHits + ".root");
    sequencer.addWriter(
        std::make_shared<ActsExamples::RootSimHitWriter>(writeHits, logLevel));
  }
}

}  // namespace

void addSimulationOptions(ActsExamples::Options::Description& desc) {
  ActsExamples::Options::addBFieldOptions(desc);
  ActsExamples::Options::addFatrasOptions(desc);
}

void setupSimulation(
    const ActsExamples::Options::Variables& vars,
    ActsExamples::Sequencer& sequencer,
    std::shared_ptr<const ActsExamples::RandomNumbers> randomNumbers,
    std::shared_ptr<const Acts::TrackingGeometry> trackingGeometry) {
  auto magneticFieldVariant = ActsExamples::Options::readBField(vars);
  std::visit(
      [&](auto&& inputField) {
        using magnetic_field_t =
            typename std::decay_t<decltype(inputField)>::element_type;
        Acts::SharedBField<magnetic_field_t> magneticField(inputField);
        setupSimulationAlgorithms(vars, sequencer, randomNumbers,
                                  trackingGeometry, std::move(magneticField));
      },
      magneticFieldVariant);
}
