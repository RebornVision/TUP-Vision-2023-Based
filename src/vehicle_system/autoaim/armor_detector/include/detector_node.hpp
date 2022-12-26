/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-10-14 16:49:59
 * @LastEditTime: 2022-12-26 13:56:20
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/autoaim/armor_detector/include/detector_node.hpp
 */
#include "../../global_user/include/global_user/global_user.hpp"
#include "./armor_detector/armor_detector.hpp"

//ros
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/publisher.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>
#include <image_transport/image_transport.hpp>
#include <image_transport/publisher.hpp>
#include <image_transport/subscriber_filter.hpp>
#include <cv_bridge/cv_bridge.h>

//custom message
#include "global_interface/msg/gimbal.hpp"
#include "global_interface/msg/armor.hpp"
#include "global_interface/msg/armors.hpp"
#include "global_interface/msg/target.hpp"

using namespace global_user;
using namespace coordsolver;
namespace armor_detector
{
    class DetectorNode : public rclcpp::Node
    {
        typedef global_interface::msg::Armors ArmorsMsg;
        typedef global_interface::msg::Target TargetMsg;

    public:
        DetectorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
        ~DetectorNode();
        
    private:
        // Subscribe img. 
        std::shared_ptr<image_transport::Subscriber> img_sub_;
        // rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr img_sub;

        // Image subscriptions transport type.
        std::string transport_;

        rclcpp::Time time_start_;
        int image_width_;
        int image_height_;
        // std::shared_ptr<sensor_msgs::msg::CameraInfo> cam_info;
        // rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr cam_info_sub;

        //发布装甲信息
        // ArmorsMsg armors_info_;
        rclcpp::Publisher<TargetMsg>::SharedPtr armor_info_pub_;
    
    private:    
        // Params callback.
        std::map<std::string, int> params_map_;
        bool setParam(rclcpp::Parameter param);
        rcl_interfaces::msg::SetParametersResult paramsCallback(const std::vector<rclcpp::Parameter>& params);
        OnSetParametersCallbackHandle::SharedPtr callback_handle_;
        
    public:
        void image_callback(const sensor_msgs::msg::Image::ConstSharedPtr &img_info);
        // std::vector<Armor> detect_armors(const sensor_msgs::msg::Image::SharedPtr& img);
  
    public:
        DetectorParam detector_params_;
        DebugParam debug_;
        GyroParam gyro_params_;

        // rclcpp::Node handle;
        // image_transport::ImageTransport it;
        std::unique_ptr<Detector> detector_;
        std::unique_ptr<Detector> init_detector();

    protected:
        bool using_shared_memory_;
        SharedMemoryParam shared_memory_param_;
        std::thread read_memory_thread_; //共享内存读线程
        void run();
    };
} //namespace detector