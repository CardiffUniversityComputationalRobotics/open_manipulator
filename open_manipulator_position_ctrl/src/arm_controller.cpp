/*******************************************************************************
* Copyright 2016 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Taehun Lim (Darby) */

#include "open_manipulator_position_ctrl/arm_controller.h"

using namespace open_manipulator;

ArmController::ArmController()
    :using_gazebo_(false),
     robot_name_(""),
     joint_num_(4),
     first_dxl_id_(1),
     is_moving_(false)
{
  // Init parameter
  nh_.getParam("gazebo", using_gazebo_);
  nh_.getParam("robot_name", robot_name_);
  nh_.getParam("first_dxl_id", first_dxl_id_);
  nh_.getParam("joint_num", joint_num_);

  for (uint8_t num = 0; num < joint_num_; num++)
  {
    Joint joint;

    joint.name   = "joint" + std::to_string(num+1);
    joint.dxl_id = num + first_dxl_id_;

    joint_.push_back(joint);
  }

  planned_path_info_.waypoints = 10;
  planned_path_info_.planned_path_positions = Eigen::MatrixXd::Zero(planned_path_info_.waypoints, joint_num_);

  move_group = new moveit::planning_interface::MoveGroupInterface("arm");

  initPublisher(using_gazebo_);
  initSubscriber(using_gazebo_);

  initServer();

  if (robot_name_ == "open_manipulator_with_tb3")
    initJointPosition();
}

ArmController::~ArmController()
{
  ros::shutdown();
  return;
}

void ArmController::initJointPosition()
{
  open_manipulator_msgs::JointPose joint_positions;

  joint_positions.joint_name.push_back("joint1");
  joint_positions.joint_name.push_back("joint2");
  joint_positions.joint_name.push_back("joint3");
  joint_positions.joint_name.push_back("joint4");

  joint_positions.position.push_back(0.0);
  joint_positions.position.push_back(-1.5707);
  joint_positions.position.push_back(1.37);
  joint_positions.position.push_back(0.2258);

  target_joint_position_pub_.publish(joint_positions);
}

void ArmController::initPublisher(bool using_gazebo)
{
  if (using_gazebo)
  {
    ROS_INFO("SET Gazebo Simulation Mode(Joint)");

    for (uint8_t index = 0; index < joint_num_; index++)
    {
      gazebo_goal_joint_position_pub_[index]
        = nh_.advertise<std_msgs::Float64>(robot_name_ + "/" + joint_[index].name + "_position/command", 10);
    }
  }

  target_joint_position_pub_ = nh_.advertise<open_manipulator_msgs::JointPose>(robot_name_ + "/joint_pose", 10);

  arm_state_pub_ = nh_.advertise<open_manipulator_msgs::State>(robot_name_ + "/state", 10);
}

void ArmController::initSubscriber(bool using_gazebo)
{
  target_joint_pose_sub_ = nh_.subscribe(robot_name_ + "/joint_pose", 10,
                                         &ArmController::targetJointPoseMsgCallback, this);

  target_kinematics_pose_sub_ = nh_.subscribe(robot_name_ + "/kinematics_pose", 10,
                                         &ArmController::targetKinematicsPoseMsgCallback, this);

  display_planned_path_sub_ = nh_.subscribe("/move_group/display_planned_path", 10,
                                            &ArmController::displayPlannedPathMsgCallback, this);
}

void ArmController::initServer()
{
  get_joint_pose_server_      = nh_.advertiseService(robot_name_ + "/get_joint_pose", &ArmController::getJointPositionMsgCallback, this);
  get_kinematics_pose_server_ = nh_.advertiseService(robot_name_ + "/get_kinematics_pose", & ArmController::getKinematicsPoseMsgCallback, this);
}

bool ArmController::getJointPositionMsgCallback(open_manipulator_msgs::GetJointPose::Request &req,
                                                open_manipulator_msgs::GetJointPose::Response &res)
{
  ros::AsyncSpinner spinner(1);
  spinner.start();

  const std::vector<std::string> &joint_names = move_group->getJointNames();
  std::vector<double> joint_values = move_group->getCurrentJointValues();

  for (std::size_t i = 0; i < joint_names.size(); ++i)
  {
    ROS_INFO("%s: %f", joint_names[i].c_str(), joint_values[i]);

    res.joint_pose.joint_name.push_back(joint_names[i]);
    res.joint_pose.position.push_back(joint_values[i]);
  }

  spinner.stop();
}

bool ArmController::getKinematicsPoseMsgCallback(open_manipulator_msgs::GetKinematicsPose::Request &req,
                                                 open_manipulator_msgs::GetKinematicsPose::Response &res)
{
  ros::AsyncSpinner spinner(1);
  spinner.start();

  const std::string &pose_reference_frame = move_group->getPoseReferenceFrame();
  ROS_INFO("Pose Reference Frame = %s", pose_reference_frame.c_str());

  std::vector<double> joint_values = move_group->getCurrentRPY();

  ROS_INFO("R: %f",joint_values[0]);
  ROS_INFO("P: %f",joint_values[1]);
  ROS_INFO("Y: %f",joint_values[2]);

  geometry_msgs::PoseStamped current_pose = move_group->getCurrentPose();

  res.header                     = current_pose.header;
  res.kinematics_pose.group_name = "arm";
  res.kinematics_pose.pose       = current_pose.pose;

  spinner.stop();
}

void ArmController::targetJointPoseMsgCallback(const open_manipulator_msgs::JointPose::ConstPtr &msg)
{
  ros::AsyncSpinner spinner(1);
  spinner.start();

  const robot_state::JointModelGroup *joint_model_group = move_group->getCurrentState()->getJointModelGroup("arm");

  moveit::core::RobotStatePtr current_state = move_group->getCurrentState();

  std::vector<double> joint_group_positions;
  current_state->copyJointGroupPositions(joint_model_group, joint_group_positions);

  for (uint8_t index = 0; index < joint_num_; index++)
  {
    if (msg->joint_name[index] == ("joint" + std::to_string((index+1))))
    {
      joint_group_positions[index] = msg->position[index];
    }
  }

  move_group->setJointValueTarget(joint_group_positions);

  // move_group->setMaxVelocityScalingFactor(0.1);
  // move_group->setMaxAccelerationScalingFactor(0.01);

  moveit::planning_interface::MoveGroupInterface::Plan my_plan;

  if (is_moving_ == false)
  {
    bool success = (move_group->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);

    if (success) move_group->move();
    else ROS_WARN("Planning (joint space goal) is FAILED");
  }
  else
    ROS_WARN("ROBOT IS WORKING");

  spinner.stop();
}

void ArmController::targetKinematicsPoseMsgCallback(const open_manipulator_msgs::KinematicsPose::ConstPtr &msg)
{
  ros::AsyncSpinner spinner(1);
  spinner.start();

//  const robot_state::JointModelGroup *joint_model_group = move_group->getCurrentState()->getJointModelGroup("arm");

//  moveit::core::RobotStatePtr current_state = move_group->getCurrentState();

//  std::vector<double> joint_group_positions;
//  current_state->copyJointGroupPositions(joint_model_group, joint_group_positions);

//  geometry_msgs::Pose target_pose = msg->pose;

//x: -1.50741324645
//y: 2.88172450297
//z: 0.224672337579
//orientation:
//x: -0.00890373524503
//y: 0.00911380245469
//z: 0.698758375403
//w: 0.715244290371

  geometry_msgs::Pose target_pose;

  target_pose.position.x = -1.50;
  target_pose.position.y = 2.88;
  target_pose.position.z = 0.42;

  move_group->setPoseTarget(target_pose);

  // move_group->setMaxVelocityScalingFactor(0.1);
  // move_group->setMaxAccelerationScalingFactor(0.01);

  moveit::planning_interface::MoveGroupInterface::Plan my_plan;

  if (is_moving_ == false)
  {
    bool success = (move_group->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);

    if (success) move_group->move();
    else ROS_WARN("Planning (cartesian space goal) is FAILED");
  }
  else
    ROS_WARN("ROBOT IS WORKING");

  spinner.stop();
}

void ArmController::displayPlannedPathMsgCallback(const moveit_msgs::DisplayTrajectory::ConstPtr &msg)
{
  // Can't find 'grip'
  if (msg->trajectory[0].joint_trajectory.joint_names[0].find("grip") == std::string::npos)
  {
    ROS_INFO("Get ARM Planned Path");
    uint8_t joint_num = joint_num_;

    planned_path_info_.waypoints = msg->trajectory[0].joint_trajectory.points.size();

    planned_path_info_.planned_path_positions.resize(planned_path_info_.waypoints, joint_num);

    for (uint16_t point_num = 0; point_num < planned_path_info_.waypoints; point_num++)
    {
      for (uint8_t num = 0; num < joint_num; num++)
      {
        float joint_position = msg->trajectory[0].joint_trajectory.points[point_num].positions[num];

        planned_path_info_.planned_path_positions.coeffRef(point_num , num) = joint_position;
      }
    }

    all_time_steps_ = planned_path_info_.waypoints - 1;

    ros::WallDuration sleep_time(0.5);
    sleep_time.sleep();

    is_moving_  = true;    
  }
}

void ArmController::process(void)
{
  static uint16_t step_cnt = 0;
  std_msgs::Float64 goal_joint_position;
  open_manipulator_msgs::State state;

  if (is_moving_)
  {
    if (using_gazebo_)
    {
      for (uint8_t num = 0; num < joint_num_; num++)
      {
        goal_joint_position.data = planned_path_info_.planned_path_positions(step_cnt, num);
        gazebo_goal_joint_position_pub_[num].publish(goal_joint_position);
      }
    }

    if (step_cnt >= all_time_steps_)
    {
      is_moving_ = false;
      step_cnt   = 0;

      ROS_INFO("Complete Execution");
    }
    else
    {
      step_cnt++;
    }

    state.gripper = state.STOPPED;
    state.arm = state.IS_MOVING;
    arm_state_pub_.publish(state);
  }
  else
  {
    state.gripper = state.STOPPED;
    state.arm = state.STOPPED;
    arm_state_pub_.publish(state);
  }
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "joint_controller_for_OpenManipulator");

  ros::WallDuration sleep_time(3.0);
  sleep_time.sleep();

  ArmController controller;

  ros::Rate loop_rate(ITERATION_FREQUENCY);

  while (ros::ok())
  {
    controller.process();

    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}