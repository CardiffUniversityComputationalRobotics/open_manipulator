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

#ifndef OPEN_MANIPULATOR_GRIPPER_CONTROLLER_H
#define OPEN_MANIPULATOR_GRIPPER_CONTROLLER_H

#include <ros/ros.h>

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/robot_state/robot_state.h>
#include <moveit/planning_interface/planning_interface.h>

#include <moveit_msgs/DisplayTrajectory.h>

#include <vector>

#include <std_msgs/Float64.h>
#include <std_msgs/String.h>

#include <sensor_msgs/JointState.h>

#include "open_manipulator_msgs/JointPose.h"
#include "open_manipulator_msgs/KinematicsPose.h"

#include <eigen3/Eigen/Eigen>

namespace open_manipulator
{
#define LEFT_PALM   0
#define RIGHT_PALM  1

#define ITERATION_FREQUENCY 25 //Hz

typedef struct
{
  std::string name;
  uint8_t dxl_id;
} Gripper;

typedef struct
{
  uint16_t waypoints;                                  // planned number of via-points
  Eigen::MatrixXd planned_path_positions;              // planned position trajectory
} PlannedPathInfo;

class GripperController
{
 private:
  // ROS NodeHandle
  ros::NodeHandle nh_;

  // ROS Parameters
  bool using_gazebo_;
  std::string robot_name_;
  int palm_num_;
  int gripper_dxl_id_;

  // ROS Publisher
  ros::Publisher gazebo_gripper_position_pub_[2];
  ros::Publisher gripper_onoff_pub_;

  // ROS Subscribers
  ros::Subscriber gazebo_present_joint_position_sub_;
  ros::Subscriber display_planned_path_sub_;
  ros::Subscriber gripper_pose_sub_;
  ros::Subscriber gripper_onoff_sub_;

  // ROS Service Server

  // ROS Service Client

  // Gripper states
  Gripper gripper_;

  // MoveIt! interface
  moveit::planning_interface::MoveGroupInterface *move_group;
  PlannedPathInfo planned_path_info_;

  // Process state variables
  bool     is_moving_;
  uint16_t all_time_steps_;

 public:
  GripperController();
  virtual ~GripperController();

  void process(void);

 private:
  void initPublisher(bool using_gazebo);
  void initSubscriber(bool using_gazebo);

  void gazeboPresentJointPositionMsgCallback(const sensor_msgs::JointState::ConstPtr &msg);
  void displayPlannedPathMsgCallback(const moveit_msgs::DisplayTrajectory::ConstPtr &msg);

  void targetGripperPoseMsgCallback(const open_manipulator_msgs::JointPose::ConstPtr &msg);
  void gripperPositionMsgCallback(const std_msgs::String::ConstPtr &msg);
};
}

#endif /*OPEN_MANIPULATOR_GRIPPER_CONTROLLER_H*/