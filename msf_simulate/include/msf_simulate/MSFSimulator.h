/*
 * MSFSimulator.h
 *
 *  Created on: Jun 30, 2013
 *      Author: slynen
 */

#ifndef MSFSIMULATOR_H_
#define MSFSIMULATOR_H_

#include <Eigen/Dense>
#include <mav_planning_utils/path_planning.h>
#include <mav_planning_utils/conversions.h>
#include <mav_planning_utils/mav_state.h>

#include <msf_core/msf_sensormanagerROS.h>
#include <msf_core/msf_IMUHandler_ROS.h>
#include <msf_core/msf_core.h>
#include "msf_statedef.hpp"

#include <msf_simulate/MSFSimulatorOptions.h>

using namespace mav_planning_utils;
using namespace mav_planning_utils::path_planning;

namespace msf_simulate {

class MSFSimulator {
 private:
  MSFSimulatorOptions options_;
  std::vector<Vertex4D> sv_;
  Path4D<12> path_;
  bool optimization_done_;
  void generateRandomTrajectory();
 public:
  typedef std::vector<msf_updates::EKFStatePtr> MotionVector;

  MSFSimulator(const MSFSimulatorOptions& options = MSFSimulatorOptions());
  MSFSimulator(const std::vector<Vertex4D>& sv, const MSFSimulatorOptions& options = MSFSimulatorOptions());
  virtual ~MSFSimulator();

  void getMotion(MotionVector& motion_out);
  void updatePathWithConstraint(int continuity, const Vertex1D & max_p,
                                const Vertex1D & max_yaw, double time_multiplier = 0.1, double tol = 1e12);
};

void MSFSimulator::getMotion(MotionVector& motion_out) {

  //sampling rate == imu rate
  double dt = 1.0 / options_.imu_rate_hz_;

  if (!optimization_done_) {
    throw std::runtime_error("You must call updatePathWithConstraint before sampling the path");
    return;
  }

  Motion4D<5, 2>::Vector data;
  path_.sample<5, 2>(data, dt);

  motion_out.reserve(data.size());

  for (size_t i = 0; i < data.size(); ++i){

    MavState mavstate;
    motion4dToMulticopter(mavstate, data.at(i));
    msf_updates::EKFStatePtr state(new msf_updates::EKFState);

    //copy simulated values to state variable
    state->set<msf_updates::StateDefinition::p>(mavstate.p);
    state->set<msf_updates::StateDefinition::v>(mavstate.v);
    state->set<msf_updates::StateDefinition::q>(mavstate.q);
    state->time = dt * i;

    //create noise
    Eigen::Matrix<double, 3, 1> accelerometer_noise;
    accelerometer_noise.setRandom().dot(options_.accelerometer_noise_sigma_);
    Eigen::Matrix<double, 3, 1> gyroscope_noise;
    gyroscope_noise.setRandom().dot(options_.gyroscope_noise_sigma_);

    //add noise and bias to true accelerations
    state->a_m = options_.accelerometer_bias_ + accelerometer_noise + mavstate.a_b;
    state->w_m = options_.gyroscope_bias_ + gyroscope_noise + mavstate.w_b;

    //copy also the true accelerometer and gyro readings
//    state->set<msf_updates::StateDefinition::a_gt>(mavstate.a_b);
//    state->set<msf_updates::StateDefinition::w_gt>(mavstate.w_b);

    motion_out.push_back(state);
  }
}

} /* namespace msf_simulate */
#endif /* MSFSIMULATOR_H_ */
