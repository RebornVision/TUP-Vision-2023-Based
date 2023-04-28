/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-10-14 17:11:03
 * @LastEditTime: 2023-04-16 15:24:57
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/autoaim/armor_detector/src/detector_node.cpp
 */
#include "../include/detector_node.hpp"

using namespace message_filters;
using namespace message_filters;
using namespace std::placeholders;
namespace armor_detector
{
    DetectorNode::DetectorNode(const rclcpp::NodeOptions& options)
    : Node("armor_detector", options), my_sync_policy_(MySyncPolicy(3))
    : Node("armor_detector", options), my_sync_policy_(MySyncPolicy(3))
    {
        RCLCPP_WARN(this->get_logger(), "Starting detector node...");

        try
        {   //detector类初始化
            this->detector_ = initDetector();
        }
        catch(const std::exception& e)
        {
            RCLCPP_FATAL(this->get_logger(), "Fatal while initializing detector class: %s", e.what());
        }

        if (!detector_->is_init_)
        if (!detector_->is_init_)
        {
            RCLCPP_INFO(this->get_logger(), "Initializing network model...");
            detector_->armor_detector_.initModel(path_params_.network_path);
            detector_->coordsolver_.loadParam(path_params_.camera_param_path, path_params_.camera_name);
            if(detector_->is_save_data_)
            {
                detector_->data_save_.open(path_params_.save_path, ios::out | ios::trunc);
                detector_->data_save_ << fixed;
            }
            detector_->is_init_ = true;
        }
        
        // 同步通信/异步通信
        this->declare_parameter<bool>("sync_transport", false);
        bool sync_transport = this->get_parameter("sync_transport").as_bool();
        
        // 同步通信/异步通信
        this->declare_parameter<bool>("sync_transport", false);
        bool sync_transport = this->get_parameter("sync_transport").as_bool();

        // QoS    
        // QoS    
        rclcpp::QoS qos(0);
        qos.keep_last(1);
        qos.keep_last(1);
        qos.reliable();
        qos.transient_local();
        qos.transient_local();
        qos.durability_volatile();
        // qos.lifespan();
        // qos.deadline();
        // qos.best_effort();
        // qos.durability();

        // qos.lifespan();
        // qos.deadline();
        // qos.best_effort();
        // qos.durability();

        rmw_qos_profile_t rmw_qos(rmw_qos_profile_default);
        rmw_qos.depth = 1;

        time_start_ = detector_->steady_clock_.now();

        // Create an image transport object.
        // auto it = image_transport::ImageTransport();

        time_start_ = detector_->steady_clock_.now();

        // Create an image transport object.
        // auto it = image_transport::ImageTransport();

        // target info pub.
        armor_info_pub_ = this->create_publisher<AutoaimMsg>("/armor_detector/armor_msg", qos);
        detections_pub_ = this->create_publisher<global_interface::msg::DetectionArray>("/armor_detector/detections", qos);
        if (debug_.using_imu)
        detections_pub_ = this->create_publisher<global_interface::msg::DetectionArray>("/armor_detector/detections", qos);
        if (debug_.using_imu)
        {
            RCLCPP_INFO(this->get_logger(), "Using imu...");
            serial_msg_.imu.header.frame_id = "imu_link";
            this->declare_parameter<double>("bullet_speed", 28.0);
            this->get_parameter("bullet_speed", serial_msg_.bullet_speed);
            serial_msg_.mode = this->declare_parameter<int>("autoaim_mode", 1);

            if (!sync_transport)
            {
                // Imu msg sub.
                serial_msg_sub_ = this->create_subscription<SerialMsg>("/serial_msg",
                    qos,
                    std::bind(&DetectorNode::sensorMsgCallback, this, _1)
                );
            }

            if (!sync_transport)
            {
                // Imu msg sub.
                serial_msg_sub_ = this->create_subscription<SerialMsg>("/serial_msg",
                    qos,
                    std::bind(&DetectorNode::sensorMsgCallback, this, _1)
                );
            }
        }
         
        // CameraType camera_type;
        this->declare_parameter<int>("camera_type", DaHeng);
        int camera_type = this->get_parameter("camera_type").as_int();
        // Subscriptions transport type.
        std::string transport_type = "raw";
        // Image size.
        image_size_ = image_info_.image_size_map[camera_type];
        // image sub.
        std::string camera_topic = image_info_.camera_topic_map[camera_type];
        if (sync_transport)
        std::string transport_type = "raw";
        // Image size.
        image_size_ = image_info_.image_size_map[camera_type];
        // image sub.
        std::string camera_topic = image_info_.camera_topic_map[camera_type];
        if (sync_transport)
        {
            // Create serial msg subscriber.
            serial_msg_sync_sub_ = std::make_shared<message_filters::Subscriber<SerialMsg>>(this, "/serial_msg", rmw_qos);

            // Create image subscriber.
            img_msg_sync_sub_ = std::make_shared<message_filters::Subscriber<sensor_msgs::msg::Image>>(this, camera_topic, rmw_qos);

            // Create synchronous timer.
            my_sync_policy_.setInterMessageLowerBound(0, rclcpp::Duration(0, 1e7));
            my_sync_policy_.setInterMessageLowerBound(1, rclcpp::Duration(0, 1e7));
            my_sync_policy_.setMaxIntervalDuration(rclcpp::Duration(0, 3e7));
            // sync_ = std::make_shared<message_filters::TimeSynchronizer<sensor_msgs::msg::Image, SerialMsg>>(*img_msg_sync_sub_, *serial_msg_sync_sub_, 0.005);
            sync_ = std::make_shared<message_filters::Synchronizer<MySyncPolicy>>(MySyncPolicy(my_sync_policy_), *img_msg_sync_sub_, *serial_msg_sync_sub_);

            // Register a callback function to process.
            sync_->registerCallback(std::bind(&DetectorNode::syncCallback, this, _1, _2));
            // Create serial msg subscriber.
            serial_msg_sync_sub_ = std::make_shared<message_filters::Subscriber<SerialMsg>>(this, "/serial_msg", rmw_qos);

            // Create image subscriber.
            img_msg_sync_sub_ = std::make_shared<message_filters::Subscriber<sensor_msgs::msg::Image>>(this, camera_topic, rmw_qos);

            // Create synchronous timer.
            my_sync_policy_.setInterMessageLowerBound(0, rclcpp::Duration(0, 1e7));
            my_sync_policy_.setInterMessageLowerBound(1, rclcpp::Duration(0, 1e7));
            my_sync_policy_.setMaxIntervalDuration(rclcpp::Duration(0, 3e7));
            // sync_ = std::make_shared<message_filters::TimeSynchronizer<sensor_msgs::msg::Image, SerialMsg>>(*img_msg_sync_sub_, *serial_msg_sync_sub_, 0.005);
            sync_ = std::make_shared<message_filters::Synchronizer<MySyncPolicy>>(MySyncPolicy(my_sync_policy_), *img_msg_sync_sub_, *serial_msg_sync_sub_);

            // Register a callback function to process.
            sync_->registerCallback(std::bind(&DetectorNode::syncCallback, this, _1, _2));

            RCLCPP_WARN(this->get_logger(), "Synchronously...");
            RCLCPP_WARN(this->get_logger(), "Synchronously...");
        }
        else
        {
            img_sub_ = std::make_shared<image_transport::Subscriber>(image_transport::create_subscription(this, camera_topic,
                std::bind(&DetectorNode::imageCallback, this, _1), transport_type, rmw_qos));
        }

        bool debug = false;
        this->declare_parameter<bool>("debug", true);
        this->get_parameter("debug", debug);
        if (debug)
        if (debug)
        {
            RCLCPP_WARN_ONCE(this->get_logger(), "debug...");
            //动态调参回调
            callback_handle_ = this->add_on_set_parameters_callback(std::bind(&DetectorNode::paramsCallback, this, _1));
        }
    }

    DetectorNode::~DetectorNode()
    {
    }

    void DetectorNode::syncCallback(const sensor_msgs::msg::Image::ConstSharedPtr& img_msg, const SerialMsg::ConstSharedPtr& serial_msg)
    {
        rclcpp::Time time = img_msg->header.stamp;
        rclcpp::Time now = this->get_clock()->now();
        double dura = (now.nanoseconds() - time.nanoseconds()) / 1e6;
        RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 500, "delay:%.2fms", dura);
        // if ((dura) > 20.0)
        //     return;
        TaskData src;
        // Convert the image to opencv format.
        // cv_bridge::CvImagePtr cv_ptr;
        try
        {
            src.img = cv_bridge::toCvShare(img_msg, "bgr8")->image;
            src.timestamp = img_msg->header.stamp.nanosec;
            src.bullet_speed = serial_msg->bullet_speed;
            src.mode = serial_msg->mode;
            src.quat.w() = serial_msg->imu.orientation.w;
            src.quat.x() = serial_msg->imu.orientation.x;
            src.quat.y() = serial_msg->imu.orientation.y;
            src.quat.z() = serial_msg->imu.orientation.z;
        }
        catch(const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
            return;
        }
        
        // RCLCPP_WARN(this->get_logger(), "mode:%d", src.mode);
        ObjHPMsg obj_hp_msg;
        if (src.mode == SENTRY_NORMAL)
        {
            // auto dt = (this->get_clock()->now() - obj_hp_msg_.header.stamp).nanoseconds() / 1e6;
            // if (dt > 500)
            // {
                // RCLCPP_WARN(this->get_logger(), "obj hp msg is timeout: %.2fms...", dt);
                obj_hp_msg_mutex_.lock();
                obj_hp_msg = obj_hp_msg_;
                obj_hp_msg_mutex_.unlock();
            // }
        }

        AutoaimMsg target_info;
        bool is_target_lost = true;
        try
        {
            param_mutex_.lock();
            // Target detector. 
            if (!detector_->armor_detect(src, is_target_lost))
            {
                RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "No target...");
            }
            else
            {   // Target spinning detector. 
                if (src.mode == SENTRY_NORMAL)
                {
                    if (!detector_->gyro_detector(src, target_info, obj_hp_msg))
                    {
                        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "[SENTRY MODE]: Not spinning...");
                    }
                    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 250, "Spinning detecting...");
                }
                else
                {
                    if (!detector_->gyro_detector(src, target_info))
                    {
                        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Not spinning...");
                    }
                    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 250, "Spinning detecting...");
                }
            }
            param_mutex_.unlock();
        }
        catch(const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "Detector node errror: %s", e.what());
        }

        target_info.header.frame_id = "gimbal_link";
        target_info.header.stamp = img_msg->header.stamp;
        target_info.mode = src.mode;
        target_info.timestamp = src.timestamp;
        target_info.quat_imu = serial_msg->imu.orientation;
        target_info.is_target_lost = is_target_lost;
        armor_info_pub_->publish(std::move(target_info));
        
        debug_.show_img = this->get_parameter("show_img").as_bool();
        if(debug_.show_img)
        {
            cv::namedWindow("dst", cv::WINDOW_AUTOSIZE);
            cv::imshow("dst", src.img);
            cv::waitKey(1);
        }
    }

    void DetectorNode::detect(TaskData& src, rclcpp::Time stamp)
    {
        serial_msg_mutex_.lock();
        if (debug_.using_imu)
        {
            auto dt = (this->get_clock()->now() - serial_msg_.imu.header.stamp).nanoseconds() / 1e6;
            putText(src.img, "IMU_DELAY:" + to_string(dt) + "ms", cv::Point2i(50, 80), cv::FONT_HERSHEY_SIMPLEX, 1, {0, 255, 255});
            // if(dt > 50)
            // {
            //     src.mode = serial_msg_.mode;
            //     src.bullet_speed = serial_msg_.bullet_speed;
            //     detector_->debug_params_.using_imu = false;
            // }
            // else
            // {
                src.bullet_speed = serial_msg_.bullet_speed;
                src.mode = serial_msg_.mode;
                src.quat.w() = serial_msg_.imu.orientation.w;
                src.quat.x() = serial_msg_.imu.orientation.x;
                src.quat.y() = serial_msg_.imu.orientation.y;
                src.quat.z() = serial_msg_.imu.orientation.z;
                // detector_->debug_params_.using_imu = true;
                RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 500, "bulletSpd:%.2f", src.bullet_speed);
            // }
        }
        serial_msg_mutex_.unlock(); 
        
        // RCLCPP_WARN(this->get_logger(), "mode:%d", src.mode);

        AutoaimMsg target_info;
        Eigen::Vector2d tracking_angle = {0.0, 0.0};
        Eigen::Matrix3d rmat_imu = Eigen::Matrix3d::Identity();
        bool is_target_lost = true;
        param_mutex_.lock();
        if (detector_->armor_detect(src, is_target_lost))
        {   
            global_interface::msg::DetectionArray detection_array;
            detection_array.header = img_header_;
            detection_array.header.frame_id = detection_array.header.frame_id + "_frame";
            for (auto armor : detector_->new_armors_)
            {
                global_interface::msg::Detection detection;
                detection.header = img_header_;
                detection.header.frame_id = detection.header.frame_id + "_frame";
                detection.conf = armor.conf;
                detection.type = armor.key;
                detection.center.position.x = armor.armor3d_cam[0];
                detection.center.position.y = armor.armor3d_cam[1];
                detection.center.position.z = armor.armor3d_cam[2];
                detection_array.detections.push_back(detection);
            }
            detections_pub_->publish(detection_array);
            if (detector_->gyro_detector(src, target_info))
            {
                // RCLCPP_INFO(this->get_logger(), "Spinning detector...");
                // if(debug_.using_imu && detector_->debug_params_.using_imu)
                if (debug_.using_imu)
                {
                    if (!detector_->gyro_detector(src, target_info))
                    {
                        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 500, "Not spinning...");
                    }
                    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 500, "Spinning detecting...");
                }

                rmat_imu = src.quat.toRotationMatrix();
                Eigen::Vector3d armor_3d_cam = {target_info.aiming_point_cam.x, target_info.aiming_point_cam.y, target_info.aiming_point_cam.z};
                tracking_angle = detector_->coordsolver_.getAngle(armor_3d_cam, rmat_imu);
                // RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 40, "target info_cam: %lf %lf %lf", target_info.aiming_point_cam.x, target_info.aiming_point_cam.y, target_info.aiming_point_cam.z);
                // RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 40, "target info_world: %lf %lf %lf", target_info.aiming_point_world.x, target_info.aiming_point_world.y, target_info.aiming_point_world.z);
            }
            param_mutex_.unlock();
        }
        catch(const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "Detector node errror: %s", e.what());
        }

        if (is_target_lost)
        {
            target_info.aiming_point_cam.x = 0;
            target_info.aiming_point_cam.y = 0;
            target_info.aiming_point_cam.z = 0;
            target_info.aiming_point_world.x = 0;
            target_info.aiming_point_world.y = 0;
            target_info.aiming_point_world.z = 0;
        }
        // else
        // {
        //     Eigen::Vector3d rpy_raw = {0, 0, 0};
        //     Eigen::AngleAxisd rollAngle(Eigen::AngleAxisd(rpy_raw[2], Eigen::Vector3d::UnitX()));
        //     Eigen::AngleAxisd pitchAngle(Eigen::AngleAxisd(rpy_raw[1], Eigen::Vector3d::UnitY()));
        //     Eigen::AngleAxisd yawAngle(Eigen::AngleAxisd(rpy_raw[0], Eigen::Vector3d::UnitZ()));
        //     Eigen::Matrix3d rmat = yawAngle * pitchAngle * rollAngle;
        // }

        param_mutex_.unlock();
        target_info.is_target_lost = is_target_lost;
        target_info.header.frame_id = "gimbal_link";
        target_info.header.stamp = stamp;
        target_info.timestamp = stamp.nanoseconds();
        // RCLCPP_INFO(this->get_logger(), "timestamp:%.8f", target_info.timestamp / 1e9);

        // if (target_info.spinning_switched)
            // cout << "spinning_switched" << endl;

        armor_info_pub_->publish(std::move(target_info));
        
        debug_.show_img = this->get_parameter("show_img").as_bool();
        if (debug_.show_img)
        {
            char ch[50];
            sprintf(ch, "pitch_angle:%.2f yaw_angle:%.2f", tracking_angle[1], tracking_angle[0]);
            std::string angle_str = ch;
            putText(src.img, angle_str, {src.img.size().width / 2 + 50, 30}, cv::FONT_HERSHEY_SIMPLEX, 1, {0, 255, 255});

            cv::namedWindow("dst", cv::WINDOW_AUTOSIZE);
            cv::imshow("dst", src.img);
            cv::waitKey(1);
        }
    }

    void DetectorNode::objHPMsgCallback(const ObjHPMsg& obj_hp_msg)
    {
        obj_hp_msg_mutex_.lock();
        obj_hp_msg_ = obj_hp_msg;
        obj_hp_msg_.header.stamp = this->get_clock()->now();
        obj_hp_msg_mutex_.unlock();
        return;
    }

    /**
     * @brief 传感器消息回调（目前是陀螺仪数据）
     * 
     * @param serial_msg 
     */
    void DetectorNode::sensorMsgCallback(const SerialMsg& serial_msg)
    {
        serial_msg_mutex_.lock();
        serial_msg_.imu.header.stamp = this->get_clock()->now();
        if(serial_msg.bullet_speed > 10)
            serial_msg_.bullet_speed = serial_msg.bullet_speed;
        if(serial_msg.mode == 1 || serial_msg.mode == 2)
            serial_msg_.mode = serial_msg.mode;
        serial_msg_.imu = serial_msg.imu;
        serial_msg_mutex_.unlock();
        return;
    }

    /**
     * @brief 图像数据回调
     * 
     * @param img_info 图像传感器数据
     */
    void DetectorNode::imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr &img_info)
    {
        // RCLCPP_INFO(this->get_logger(), "image callback...");
        if(!img_info)
            return;
        img_header_ = img_info->header;
        rclcpp::Time time = img_info->header.stamp;
        rclcpp::Time now = this->get_clock()->now();
        double dura = (now.nanoseconds() - time.nanoseconds()) / 1e6;
        // RCLCPP_WARN(this->get_logger(), "delay:%.2fms", dura);
        if ((dura) > 20.0)
            return;

        TaskData src;
        rclcpp::Time stamp = img_info->header.stamp;
        src.timestamp = stamp.nanoseconds();
        src.img = cv_bridge::toCvShare(img_info, "bgr8")->image;
        // img.copyTo(src.img);
        // RCLCPP_INFO(this->get_logger(), "src_timestamp:%.8f", src.timestamp / 1e9);
        
        if (debug_.show_img)
        rclcpp::Time stamp = img_info->header.stamp;
        src.timestamp = stamp.nanoseconds();
        src.img = cv_bridge::toCvShare(img_info, "bgr8")->image;
        // img.copyTo(src.img);
        // RCLCPP_INFO(this->get_logger(), "src_timestamp:%.8f", src.timestamp / 1e9);
        
        if (debug_.show_img)
        {
            char ch[25];
            sprintf(ch, "img_trans_delay:%.2fms", dura);
            std::string delay_str = ch;
            putText(src.img, delay_str, {src.img.size().width / 5 - 40, 30}, cv::FONT_HERSHEY_SIMPLEX, 1, {0, 125, 255});
            char ch[25];
            sprintf(ch, "img_trans_delay:%.2fms", dura);
            std::string delay_str = ch;
            putText(src.img, delay_str, {src.img.size().width / 5 - 40, 30}, cv::FONT_HERSHEY_SIMPLEX, 1, {0, 125, 255});
        }

        //目标检测接口函数
        detect(src, stamp);

        //目标检测接口函数
        detect(src, stamp);
    }

    /**
     * @brief 参数回调函数
     * 
     * @param params 参数服务器参数（发生改变的参数）
     * @return rcl_interfaces::msg::SetParametersResult 
     */
    rcl_interfaces::msg::SetParametersResult DetectorNode::paramsCallback(const std::vector<rclcpp::Parameter>& params)
    { 
        rcl_interfaces::msg::SetParametersResult result;
        result.successful = false;
        result.reason = "debug";
        result.successful = updateParam();
        result.successful = detector_->coordsolver_.setStaticAngleOffset(detector_params_.angle_offset);
        
        param_mutex_.lock();
        detector_->detector_params_ = this->detector_params_;
        detector_->spinning_detector_.gyro_params_ = this->gyro_params_;
        detector_->debug_params_ = this->debug_;
        param_mutex_.unlock();
        return result;
    }

    /**
     * @brief 初始化detector类
     * 
     * @return std::unique_ptr<Detector> 
     */
    std::unique_ptr<Detector> DetectorNode::initDetector()
    {
        //Detector params.
        this->declare_parameter<int>("armor_type_wh_thres", 3);
        this->declare_parameter<int>("max_lost_cnt", 5);
        this->declare_parameter<int>("max_armors_cnt", 8);
        this->declare_parameter<int>("max_v", 8);
        this->declare_parameter<double>("no_crop_thres", 1e-2);
        this->declare_parameter<int>("hero_danger_zone", 4);
        this->declare_parameter<bool>("color", true);
        this->declare_parameter<double>("no_crop_ratio", 2e-3);
        this->declare_parameter<double>("full_crop_ratio", 1e-4);
        this->declare_parameter<double>("armor_roi_expand_ratio_width", 1.1);
        this->declare_parameter<double>("armor_roi_expand_ratio_height", 1.5);
        this->declare_parameter<double>("armor_conf_high_thres", 0.82);
        this->declare_parameter<double>("yaw_angle_offset", 0.0);
        this->declare_parameter<double>("pitch_angle_offset", 0.0);
        
        //TODO:Set by your own path.
        this->declare_parameter("camera_name", "KE0200110075"); //相机型号
        this->declare_parameter("camera_param_path", "/config/camera.yaml");
        this->declare_parameter("network_path", "/model/opt-0527-002.xml");
        this->declare_parameter("save_path", "/data/info.txt");
        
        //Debug.
        this->declare_parameter("debug_without_com", true);
        this->declare_parameter("using_imu", false);
        this->declare_parameter("using_roi", true);
        this->declare_parameter("show_aim_cross", false);
        this->declare_parameter("show_img", false);
        this->declare_parameter("detect_red", true);
        this->declare_parameter("show_fps", false);
        this->declare_parameter("print_letency", false);
        this->declare_parameter("print_target_info", false);
        this->declare_parameter("show_all_armors", false);
        this->declare_parameter("save_data", false);
        this->declare_parameter("save_dataset", false);
        
        //Gyro params.
        this->declare_parameter<int>("max_dead_buffer", 2) ;
        this->declare_parameter<int>("max_delta_t", 100);
        this->declare_parameter<double>("switch_max_dt", 10000.0);
        this->declare_parameter<double>("max_delta_dist", 0.3);
        this->declare_parameter<double>("anti_spin_judge_high_thres", 2e4);
        this->declare_parameter<double>("anti_spin_judge_low_thres", 2e3);
        this->declare_parameter<double>("anti_spin_max_r_multiple", 4.5);
        
        
        //Update param from param server.
        updateParam();

        Eigen::Vector2d angle_offset = {0.0, 0.0};
        angle_offset[0] = this->get_parameter("yaw_angle_offset").as_double();
        angle_offset[1] = this->get_parameter("pitch_angle_offset").as_double();

        return std::make_unique<Detector>(path_params_, detector_params_, debug_, gyro_params_, angle_offset);
    }

    /**
     * @brief 更新参数
     * 
     * @return true 
     * @return false 
     */
    bool DetectorNode::updateParam()
    {
        detector_params_.armor_type_wh_thres = this->get_parameter("armor_type_wh_thres").as_int();
        detector_params_.max_lost_cnt = this->get_parameter("max_lost_cnt").as_int();
        detector_params_.max_armors_cnt = this->get_parameter("max_armors_cnt").as_int();
        detector_params_.max_v = this->get_parameter("max_v").as_int();
        detector_params_.no_crop_thres = this->get_parameter("no_crop_thres").as_double();
        detector_params_.hero_danger_zone = this->get_parameter("hero_danger_zone").as_int();
        bool det_red = this->get_parameter("color").as_bool();
        if(det_red)
            detector_params_.color = RED;
        else
            detector_params_.color = BLUE;
        detector_params_.no_crop_ratio = this->get_parameter("no_crop_ratio").as_double();
        detector_params_.full_crop_ratio = this->get_parameter("full_crop_ratio").as_double();
        detector_params_.armor_roi_expand_ratio_width = this->get_parameter("armor_roi_expand_ratio_width").as_double();
        detector_params_.armor_roi_expand_ratio_height = this->get_parameter("armor_roi_expand_ratio_height").as_double();
        detector_params_.armor_conf_high_thres = this->get_parameter("armor_conf_high_thres").as_double();
        detector_params_.angle_offset[0] = this->get_parameter("yaw_angle_offset").as_double();
        detector_params_.angle_offset[1] = this->get_parameter("pitch_angle_offset").as_double();

        debug_.detect_red = this->get_parameter("detect_red").as_bool();
        debug_.debug_without_com  = this->get_parameter("debug_without_com").as_bool();
        debug_.show_aim_cross = this->get_parameter("show_aim_cross").as_bool();
        debug_.show_img = this->get_parameter("show_img").as_bool();
        debug_.using_imu = this->get_parameter("using_imu").as_bool();
        debug_.using_roi = this->get_parameter("using_roi").as_bool();
        debug_.show_fps = this->get_parameter("show_fps").as_bool();
        debug_.print_letency = this->get_parameter("print_letency").as_bool();
        debug_.print_target_info = this->get_parameter("print_target_info").as_bool();
        debug_.show_all_armors = this->get_parameter("show_all_armors").as_bool();
        debug_.save_data = this->get_parameter("save_data").as_bool();
        debug_.save_dataset = this->get_parameter("save_dataset").as_bool();

        gyro_params_.anti_spin_judge_high_thres = this->get_parameter("anti_spin_judge_high_thres").as_double();
        gyro_params_.anti_spin_judge_low_thres = this->get_parameter("anti_spin_judge_low_thres").as_double();
        gyro_params_.anti_spin_max_r_multiple = this->get_parameter("anti_spin_max_r_multiple").as_double();
        gyro_params_.hero_danger_zone = this->get_parameter("hero_danger_zone").as_int();
        gyro_params_.max_dead_buffer = this->get_parameter("max_dead_buffer").as_int() ;
        gyro_params_.max_delta_dist = this->get_parameter("max_delta_dist").as_double();
        gyro_params_.max_delta_t = this->get_parameter("max_delta_t").as_int();
        gyro_params_.switch_max_dt = this->get_parameter("switch_max_dt").as_double();

        string pkg_share_directory[2] = 
        {
            {get_package_share_directory("global_user")}, 
            {get_package_share_directory("armor_detector")}
        };
        path_params_.camera_name = this->get_parameter("camera_name").as_string();
        path_params_.camera_param_path = pkg_share_directory[0] + this->get_parameter("camera_param_path").as_string();
        path_params_.network_path = pkg_share_directory[1] + this->get_parameter("network_path").as_string();
        path_params_.save_path = pkg_share_directory[0] + this->get_parameter("save_path").as_string();

        return true;
    }
} //namespace detector

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<armor_detector::DetectorNode>());
    rclcpp::shutdown();
    return 0;
}

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(armor_detector::DetectorNode)