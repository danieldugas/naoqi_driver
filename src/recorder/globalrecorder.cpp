/*
 * Copyright 2015 Aldebaran
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
 *
*/

/* ROS */
#include <std_msgs/Int32.h>
#include <tf2_msgs/TFMessage.h>

/* QI */
#include <qi/log.hpp>
qiLogCategory("ros.Recorder");

/* BOOST */
#include <boost/filesystem.hpp>

#include <ctime>
#include <sstream>

#include <alrosbridge/recorder/globalrecorder.hpp>

namespace alros
{
namespace recorder
{

  GlobalRecorder::GlobalRecorder(const std::string& prefix):
    _bag()
  , _processMutex()
  , _nameBag("")
  , _isStarted(false)
  {
    if (!prefix.empty())
    {
      _prefix = "/"+prefix+"/";
    }
    else
    {
      _prefix = "/";
    }
  }

  void GlobalRecorder::startRecord() {
    boost::mutex::scoped_lock startLock( _processMutex );
    if (!_isStarted) {
      try {
        // Get current path
        boost::filesystem::path cur_path( boost::filesystem::current_path() );

        // Get time
        time_t rawtime;
        struct tm * timeinfo;
        char buffer[80];
        std::time(&rawtime);
        timeinfo = std::localtime(&rawtime);
        std::strftime(buffer,80,"/%d-%m-%Y_%I:%M:%S",timeinfo);

        _nameBag = cur_path.string()+buffer;
        _nameBag.append(".bag");

        _bag.open(_nameBag, rosbag::bagmode::Write);
        _isStarted = true;
        std::cout << YELLOW << "The bag " << BOLDCYAN << _nameBag << RESETCOLOR << YELLOW << " is opened" << RESETCOLOR << std::endl;
      } catch (std::exception e){
        throw std::runtime_error(e.what());
      }
    }
    else {
      qiLogError() << "Cannot start a record. The module is already recording.";
    }
  }

  std::string GlobalRecorder::stopRecord(const std::string& robot_ip) {
    boost::mutex::scoped_lock stopLock( _processMutex );
    if (_isStarted) {
      _bag.close();
      _isStarted = false;

      std::stringstream message;
      message << "The bag " << _nameBag << " is closed. ";
      std::cout << YELLOW << "The bag " << BOLDCYAN << _nameBag << RESETCOLOR << YELLOW << " is closed" << RESETCOLOR << std::endl;

      // Check if we are on a robot
      char* current_path;
      current_path = getenv("HOME");
      std::string cp = current_path;
      if (!(cp.find("nao") == std::string::npos)) {
        std::cout << BOLDRED << "To download this bag on your computer:" << RESETCOLOR << std::endl
                     << GREEN << "\t$ scp nao@" << robot_ip << ":" << _nameBag << " <LOCAL_PATH>" << RESETCOLOR
                        << std::endl;
        message << "To download this bag on your computer: "
                << "scp nao@" << robot_ip << ":" << _nameBag << " <LOCAL_PATH>";
      }

      _nameBag.clear();
      return message.str();
    }
    else {
      qiLogError() << "Cannot stop recording while it has not been started.";
      return "Cannot stop recording while it has not been started.";
    }
  }

  bool GlobalRecorder::isStarted() {
    return _isStarted;
  }

  void GlobalRecorder::write(const std::string& topic, const std::vector<geometry_msgs::TransformStamped>& msgtf) {
    std::string ros_topic;
    if (topic[0]!='/')
    {
      ros_topic = _prefix+topic;
    }
    else
    {
      ros_topic = topic;
    }
    tf2_msgs::TFMessage message;
    ros::Time now = ros::Time::now();
    if (!msgtf[0].header.stamp.isZero()) {
      now = msgtf[0].header.stamp;
    }
    for (std::vector<geometry_msgs::TransformStamped>::const_iterator it = msgtf.begin(); it != msgtf.end(); ++it)
    {
      message.transforms.push_back(*it);
    }
    boost::mutex::scoped_lock writeLock( _processMutex );
    if (_isStarted) {
      _bag.write(ros_topic, now, message);
    }
  }

} // recorder
} // alros
