/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-10-13 23:51:58
 * @LastEditTime: 2022-12-27 15:09:48
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/autoaim/armor_detector/include/armor_detector/armor_detector.hpp
 */
#include "../../global_user/include/global_user/global_user.hpp"
#include "../../global_user/include/coordsolver.hpp"
#include "global_interface/msg/point2f.hpp"
#include "global_interface/msg/target.hpp"

#include "../inference/inference_api2.hpp"
#include "../armor_tracker/armor_tracker.hpp"
#include "../spinning_detector/spinning_detector.hpp"

//C++
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

//ros
#include <rclcpp/rclcpp.hpp>
using namespace global_user;
using namespace coordsolver;
namespace armor_detector
{
    struct DebugParam
    {
        bool debug_without_com;
        bool using_imu;
        bool using_roi;
        bool show_aim_cross;
        bool show_img;
        bool detect_red;
        bool show_all_armors;
        bool show_fps;
        bool print_letency;
        bool print_target_info;

        DebugParam()
        {
            debug_without_com = true;
            using_imu = false;
            using_roi = false;
            show_aim_cross = false;
            show_img = true;
            detect_red = true;
            show_all_armors = true;
            show_fps = true;
            print_letency = false;
            print_target_info = true; 
        }
    };

    struct DetectorParam
    {
        int dw, dh;             //letterbox对原图像resize的padding区域的宽度和高度
        float rescale_ratio;    //缩放比例 

        int armor_type_wh_thres; //大小装甲板长宽比阈值
        int max_lost_cnt;        //最大丢失目标帧数
        int max_armors_cnt;    //视野中最多装甲板数
        int max_v;         //两次预测间最大速度(m/s)
        int max_delta_t;   //使用同一预测器的最大时间间隔(ms)

        double no_crop_thres; //禁用ROI裁剪的装甲板占图像面积最大面积比值
        int hero_danger_zone; //英雄危险距离阈值，检测到有小于该距离的英雄直接开始攻击
        double no_crop_ratio;
        double full_crop_ratio;

        double max_delta_dist;
        double armor_roi_expand_ratio_width;
        double armor_roi_expand_ratio_height;
        double armor_conf_high_thres;

        Color color;
        DetectorParam()
        {
            color = RED;
            armor_type_wh_thres = 3;
            max_lost_cnt = 5;
            max_armors_cnt = 8;
            max_v = 0;
            max_delta_t = 100;
            no_crop_thres = 2e-3;
            no_crop_ratio = 2e-3;
            full_crop_ratio = 1e-4;

            hero_danger_zone = 4.0;
            armor_roi_expand_ratio_width = 1.1;
            armor_roi_expand_ratio_height = 1.5;
            armor_conf_high_thres = 0.82;
        }
    };
    
    class Detector
    {
    public:
        Detector(const std::string& camera_name, const std::string& camera_param_path, const std::string& network_path,
            const DetectorParam& detector_params_, const DebugParam& debug_params_, const GyroParam& gyro_params_);
        ~Detector();

    private:
        //debug
        DebugParam debug_params_;
        std::string camera_name;
        std::string camera_param_path;
        std::string network_path;
        DetectorParam detector_params_;

    public:
        // void run();
        bool armor_detect(global_user::TaskData &src);
        bool gyro_detector(global_user::TaskData &src, global_interface::msg::Target& target_info);

        Point2i cropImageByROI(Mat &img);
        ArmorTracker* chooseTargetTracker(vector<ArmorTracker*> trackers, int timestamp);
        int chooseTargetID(vector<Armor> &armors, int timestamp);

        void debugParams(const DetectorParam& detector_params, const DebugParam& debug_params, const GyroParam& gyro_params);
    public:
        rclcpp::Clock steady_clock_{RCL_STEADY_TIME};

        std::vector<Armor> armors;
        std::vector<Armor> last_armors;

        ofstream data_save;
        bool is_save_data;
    private:
        bool is_init;
        Armor last_armor;
        std::vector<ArmorObject> objects;
        CoordSolver coordsolver_;
        ArmorDetector detector_;
        SpinningDetector spinning_detector_;

        std::vector<ArmorTracker> trackers;
        std::multimap<std::string, ArmorTracker> trackers_map;
        std::map<string, int> new_armors_cnt_map;    //装甲板计数map，记录新增装甲板数
        
        Eigen::Matrix3d rmat_imu;
    private:
        int count;
        rclcpp::Time time_start;
        rclcpp::Time time_infer;
        rclcpp::Time time_crop;
        
        double timestamp;
        int dead_buffer_cnt;

        bool is_last_target_exists;
        bool is_target_switched;
        int lost_cnt;
        int last_timestamp;
        int prev_timestamp;
        double last_target_area;
        double last_bullet_speed;
        Point2i last_roi_center;
        Eigen::Vector3d last_aiming_point;
        
        Point2i roi_offset;
        Size2i input_size;

        void showArmors(TaskData& src);
    public:
        void setDetectorParam(const double& param, int idx);
        void setDebugParam(const bool& param, int idx);
    }; 
} //namespace detector