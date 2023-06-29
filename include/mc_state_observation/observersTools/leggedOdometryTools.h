#pragma once

#include <mc_state_observation/observersTools/measurementsTools.h>
#include <state-observation/dynamics-estimators/kinetics-observer.hpp>

namespace mc_state_observation
{
namespace leggedOdometry
{

/**
 * Interface for the implementation of legged odometry. This odometry is based on the tracking of successive contacts
 * for the estimation of the pose of the floating base of the robot.
 * The tilt cannot be estimated from this method (but the yaw can), it has to be estimated beforehand by another
 * observer. One can decide to perform flat or 6D odometry. The flat odometry considers that the robot walks on a flat
 * ground and corrects the estimated height accordingly, it is preferable in this use case.
 * The odometry manager must be initialized once all the configuration parameters are retrieved using the init function,
 * and called on every iteration with \ref LeggedOdometryManager::run(const mc_control::MCController & ctl,
 * mc_rtc::Logger & logger, sva::PTransformd & pose, sva::MotionVecd & vels, sva::MotionVecd & accs).
 **/

struct LeggedOdometryManager
{
public:
  LeggedOdometryManager() {}

private:
  ///////////////////////////////////////////////////////////////////////
  /// ------------------------------Contacts-----------------------------
  ///////////////////////////////////////////////////////////////////////
  // Enhancement of the class ContactWithSensor with the reference of the contact in the world and the force measured by
  // the associated sensor
  class LoContactWithSensor : public measurements::ContactWithSensor
  {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  private:
    LoContactWithSensor() {}

  public:
    LoContactWithSensor(int id, std::string name)
    {
      id_ = id;
      name_ = name;
      resetContact();
    }

  public:
    // reference of the contact in the world
    stateObservation::kine::Kinematics worldRefKine_;
    // indicates whether the contact can be used for the orientation odometry or not
    bool useForOrientation_ = false;
    // norm of the force measured by the sensor
    double forceNorm_ = 0.0;
    // currently estimated orientation of the contact in the world
    stateObservation::kine::Orientation currentWorldOrientation_;
  };

  class loContactWithoutSensor : public measurements::ContactWithoutSensor
  {
    // the legged odometry requires the use of contacts associated to force sensors, this class must therefore not be
    // implemented
  public:
    loContactWithoutSensor(int id, std::string name)
    {
      BOOST_ASSERT(false && "The legged odometry requires to use only contacts with sensors.");
      id_ = id;
      name_ = name;
    }

  private:
    loContactWithoutSensor()
    {
      BOOST_ASSERT(false && "The legged odometry requires to use only contacts with sensors.");
    }
  };

  ///////////////////////////////////////////////////////////////////////
  /// ------------------------Contacts Manager---------------------------
  ///////////////////////////////////////////////////////////////////////

  typedef measurements::ContactsManager<LoContactWithSensor, loContactWithoutSensor> ContactsManager;

  class LeggedOdometryContactsManager : public ContactsManager
  {
  private:
    struct sortByForce
    {
      inline bool operator()(const LoContactWithSensor & contact1, const LoContactWithSensor & contact2) const
      {
        return (contact1.forceNorm_ < contact2.forceNorm_);
      }
    };

  public:
    std::set<std::reference_wrapper<LoContactWithSensor>, sortByForce> oriOdometryContacts_;
  };

public:
  /// @brief Initializer for the odometry manager.
  /// @details Version for the contact detection using surfaces.
  /// @param ctl Controller
  /// @param robotName Name of the robot
  /// @param odometryName Name of the odometry, used in logs and in the gui.
  /// @param odometry6d Indicates if the desired odometry must be a flat or a 6D odometry.
  /// @param withNaiveYawEstimation Indicates if the orientation must be estimated by this odometry.
  /// @param contactsDetection Desired contacts detection method.
  /// @param surfacesForContactDetection Admissible surfaces for the contacts detection.
  /// @param contactsSensorDisabledInit Contacts that must not be enabled since the beginning (faulty one for example).
  /// @param contactDetectionThreshold Threshold used for the contact detection
  void init(const mc_control::MCController & ctl,
            const std::string robotName,
            const std::string & odometryName,
            const bool odometry6d,
            const bool withNaiveYawEstimation,
            const std::string contactsDetection,
            std::vector<std::string> surfacesForContactDetection,
            std::vector<std::string> contactsSensorDisabledInit,
            const double contactDetectionThreshold);

  /// @brief Initializer for the odometry manager.
  /// @details Version for the contact detection using a thresholding on the contact force sensors measurements or by
  /// direct input from the solver.
  /// @param ctl Controller
  /// @param robotName Name of the robot
  /// @param odometryName Name of the odometry, used in logs and in the gui.
  /// @param odometry6d Indicates if the desired odometry must be a flat or a 6D odometry.
  /// @param withNaiveYawEstimation Indicates if the orientation must be estimated by this odometry.
  /// @param contactsDetection Desired contacts detection method.
  /// @param contactsSensorDisabledInit Contacts that must not be enabled since the beginning (faulty one for example).
  /// @param contactDetectionThreshold Threshold used for the contact detection
  void init(const mc_control::MCController & ctl,
            const std::string robotName,
            const std::string & odometryName,
            const bool odometry6d,
            const bool withNaiveYawEstimation,
            const std::string contactsDetection,
            std::vector<std::string> contactsSensorDisabledInit,
            const double contactDetectionThreshold);

  /// @brief Core function runing the odometry.
  /// @param ctl Controller
  /// @param pose The pose of the floating base in the world that we want to update
  /// @param vels The velocities of the floating base in the world that we want to update
  /// @param accs The accelerations of the floating base in the world that we want to update
  void run(const mc_control::MCController & ctl,
           mc_rtc::Logger & logger,
           sva::PTransformd & pose,
           sva::MotionVecd & vels,
           sva::MotionVecd & accs);

  /// @brief Updates the pose of the contacts and estimates the floating base from them.
  /// @param ctl Controller.
  /// @param logger Logger.
  void updateContacts(const mc_control::MCController & ctl, mc_rtc::Logger & logger);

  /// @brief Updates the floating base kinematics given as argument by the observer.
  /// @param ctl Controller
  /// @param pose The pose of the floating base in the world that we want to update
  /// @param vels The velocities of the floating base in the world that we want to update
  /// @param accs The accelerations of the floating base in the world that we want to update
  void updateFbKinematics(const mc_control::MCController & ctl,
                          sva::PTransformd & pose,
                          sva::MotionVecd & vels,
                          sva::MotionVecd & accs);

  /// @brief Computes the reference kinematics of the newly set contact in the world.
  /// @param forceSensor The force sensor attached to the contact
  void setNewContact(const mc_rbdyn::ForceSensor forceSensor);

  /// @brief Computes the kinematics of the contact attached to the odometry robot in the world frame.
  /// @param contact Contact of which we want to compute the kinematics
  /// @param measurementsRobot Robot used only to obtain the sensors measurements.
  /// @return stateObservation::kine::Kinematics
  stateObservation::kine::Kinematics getContactKinematics(LoContactWithSensor & contact,
                                                          const mc_rbdyn::Robot & measurementsRobot);

  /// @brief Select which contacts to use for the orientation odometry
  /// @details The two contacts with the highest measured force are selected. The contacts at hands are ignored because
  /// their orientation is less trustable.
  void selectForOrientationOdometry();

  /// @brief Add the log entries corresponding to the contact.
  /// @param logger
  /// @param contactName
  void addContactLogEntries(mc_rtc::Logger & logger, const std::string & contactName);

  /// @brief Remove the log entries corresponding to the contact.
  /// @param logger
  /// @param contactName
  void removeContactLogEntries(mc_rtc::Logger & logger, const std::string & contactName);

  /// @brief Getter for the odometry robot used for the estimation.
  mc_rbdyn::Robot & odometryRobot()
  {
    return odometryRobot_->robot("odometryRobot");
  }

  /// @brief Getter for the contacts manager.
  LeggedOdometryContactsManager & contactsManager()
  {
    return contactsManager_;
  }

protected:
  // Name of the odometry, used in logs and in the gui.
  std::string odometryName_;
  // Name of the robot
  std::string robotName_;
  // Indicates if the desired odometry must be a flat or a 6D odometry.
  bool odometry6d_;
  // Indicates if the orientation must be estimated by this odometry.
  bool withNaiveYawEstimation_;
  // tracked pose of the floating base
  sva::PTransformd fbPose_ = sva::PTransformd::Identity();

private:
  LeggedOdometryContactsManager contactsManager_;
  std::shared_ptr<mc_rbdyn::Robots> odometryRobot_;
  bool detectionFromThreshold_ = false;
};

} // namespace leggedOdometry

} // namespace mc_state_observation