/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-12-20 15:56:01
 * @LastEditTime: 2022-12-31 18:28:40
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/buff/buff_detector/src/buff_detector/buff_detector.cpp
 */
#include "../../include/buff_detector/buff_detector.hpp"

namespace buff_detector
{
    Detector::Detector()
    : logger_(rclcpp::get_logger("buff_detector"))
    {
        lost_cnt_ = 0;
        is_last_target_exists_ = false;
        // input_size_ = {640,384};
        input_size_ = {640, 640};
        last_bullet_speed_ = 0;
    }

    Detector::Detector(const BuffParam& buff_param, const PathParam& path_param, const DebugParam& debug_param)
    : buff_param_(buff_param), path_param_(path_param), debug_param_(debug_param),
    logger_(rclcpp::get_logger("buff_detector"))
    {
        lost_cnt_ = 0;
        is_last_target_exists_ = false;
        input_size_ = {640, 640};
        last_bullet_speed_ = 0;
    }

    Detector::~Detector()
    {

    }

    bool Detector::run(TaskData& src, TargetInfo& target_info)
    {
        auto time_start = steady_clock_.now();

        vector<BuffObject> objects;
        // vector<Fan> fans_;
        auto input = src.img;

        // TODO:放在节点类初始化时加载
        if(!is_initialized_)
        {
            buff_detector_.initModel(path_param_.network_path);
            coordsolver_.loadParam(path_param_.camera_param_path, path_param_.camera_name);
            is_initialized_ = true;
        }

        if(!debug_param_.using_imu)
        {
            if (src.bullet_speed > 10)
            {
                double bullet_speed = 0.0;
                if (abs(src.bullet_speed - last_bullet_speed_) < 0.5 || abs(src.bullet_speed - last_bullet_speed_) > 1.5)
                {
                    bullet_speed = src.bullet_speed;
                    coordsolver_.setBulletSpeed(bullet_speed);
                    last_bullet_speed_ = bullet_speed;
                    RCLCPP_INFO(logger_, "bullet speed: %lfm/s", bullet_speed);
                }
            }
        }

        if(debug_param_.using_imu)
        {
            rmat_imu_ = src.quat.toRotationMatrix();
            RCLCPP_INFO(logger, "Using imu...");
        }
        else
        {
            rmat_imu_= Eigen::Matrix3d::Identity();
            RCLCPP_WARN(logger_, "No imu...");
        }
        
        // TODO:修复ROI
        if(debug_param_.using_roi)
        {
            roi_offset_ = cropImageByROI(input);
            RCLCPP_INFO(logger_, "Using roi...");
        }

        if(debug_param_.assist_label)
        {
            auto img_name = path_param_.path_prefix + to_string(src.timestamp) + ".jpg";
            imwrite(img_name, input);
            RCLCPP_INFO(logger_, "Auto label...");
        }

        auto time_crop = steady_clock_.now();

        if (!buff_detector_.detect(input, objects))
        {   //若未检测到目标
            lost_cnt_++;
            is_last_target_exists_ = false;
            last_target_area_ = 0;
            RCLCPP_WARN(logger_, "No buff target is detected...");
            return false;
        }

        auto time_infer = steady_clock_.now();

        // 创建扇叶对象
        for (auto object : objects)
        {
            if(debug_param_.detect_red)
                if (object.color != 1)
                    continue;
            if(!debug_param_.detect_red)
                if (object.color != 0)
                    continue;
            
            Fan fan;
            fan.id = object.cls;
            fan.color = object.color;
            fan.conf = object.prob;
            if (object.color == 0)
                fan.key = "B" + string(object.cls == 0 ? "Activated" : "Target");
            if (object.color == 1)
                fan.key = "R" + string(object.cls == 0 ? "Activated" : "Target");
            memcpy(fan.apex2d, object.apex, 5 * sizeof(cv::Point2f));
            for(int i = 0; i < 5; i++)
            {
                fan.apex2d[i] += Point2f((float)roi_offset_.x, (float)roi_offset_.y);
            }

            std::vector<Point2f> points_pic(fan.apex2d, fan.apex2d + 5);
            TargetType target_type = BUFF;

            // TODO:迭代法进行PnP解算
            auto pnp_result = coordsolver_.pnp(points_pic, rmat_imu_, target_type, SOLVEPNP_ITERATIVE);
            // auto pnp_result = coordsolver_.pnp(points_pic, rmat_imu_, target_type, SOLVEPNP_IPPE);

            fan.armor3d_cam = pnp_result.armor_cam;
            fan.armor3d_world = pnp_result.armor_world;
            fan.centerR3d_cam = pnp_result.R_cam;
            fan.centerR3d_world = pnp_result.R_world;

            fan.euler = pnp_result.euler;
            fan.rmat = pnp_result.rmat;

            fans_.push_back(fan);
        }

        // 维护Tracker队列，删除过旧的Tracker
        if (trackers_.size() != 0)
        {
            for (auto iter = trackers_.begin(); iter != trackers_.end();)
            {
                //删除元素后迭代器会失效，需先行获取下一元素
                auto next = iter;
                if (((src.timestamp - (*iter).last_timestamp) / 1e6) > buff_param_.max_delta_t)
                    next = trackers_.erase(iter);
                else
                    ++next;
                iter = next;
            }
        }

        // 分配或创建扇叶追踪器（fan tracker）
        // TODO:增加防抖
        std::vector<FanTracker> trackers_tmp;
        for (auto fan = fans_.begin(); fan != fans_.end(); ++fan)
        {
            if (trackers_.size() == 0)
            {
                FanTracker fan_tracker((*fan), src.timestamp);
                trackers_tmp.push_back(fan_tracker);
            }
            else
            {
                double min_v = 1e9;
                int min_last_delta_t = 1e9;
                bool is_best_candidate_exist = false;
                std::vector<FanTracker>::iterator best_candidate;
                for (auto iter = trackers_.begin(); iter != trackers_.end(); iter++)
                {
                    double delta_t = ((src.timestamp - (*iter).prev_timestamp) / 1e6);
                    Eigen::AngleAxisd angle_axisd;
                    double rotate_speed;
                    int sign;
                    //----------------------------计算角度,求解转速----------------------------
                    // 若该扇叶完成初始化,且隔一帧时间较短
                    if ((*iter).is_initialized && delta_t < buff_param_.max_delta_t)
                    {
                        // delta_t = src.timestamp - (*iter).prev_timestamp;
                        // 目前扇叶到上一次扇叶的旋转矩阵
                        auto relative_rmat = (*iter).prev_fan.rmat.transpose() * (*fan).rmat;
                        angle_axisd = Eigen::AngleAxisd(relative_rmat);
                        auto rotate_axis_world = (*iter).last_fan.rmat * angle_axisd.axis();
                        // auto rotate_axis_world = (*fan).rmat * angle_axisd.axis();
                        // auto rotate_axis_world = (*iter).last_fan.rmat  * angle_axisd.axis();
                        sign = ((*fan).centerR3d_world.dot(rotate_axis_world) > 0 ) ? 1 : -1;
                    }
                    else
                    {
                        delta_t = src.timestamp - (*iter).last_timestamp;
                        // 目前扇叶到上一次扇叶的旋转矩阵
                        auto relative_rmat = (*iter).last_fan.rmat.transpose() * (*fan).rmat;
                        // TODO:使用点乘判断旋转方向
                        angle_axisd = Eigen::AngleAxisd(relative_rmat);
                        auto rotate_axis_world = (*fan).rmat * angle_axisd.axis();
                        sign = ((*fan).centerR3d_world.dot(rotate_axis_world) > 0 ) ? 1 : -1;
                    }
                    RCLCPP_INFO(logger_, "Rotate direction: %d", sign);

                    // 计算角速度(rad/s)
                    delta_t = ((src.timestamp - (*iter).last_timestamp) / 1e6);
                    rotate_speed = sign * (angle_axisd.angle()) / (delta_t / 1e3);
                    RCLCPP_INFO(logger_, "Rotate speed: %lf", rotate_speed);

                    if (abs(rotate_speed) <= min_v && abs(rotate_speed) <= buff_param_.max_v && delta_t <= min_last_delta_t)
                    {
                        min_last_delta_t = delta_t;
                        min_v = rotate_speed;
                        best_candidate = iter;
                        is_best_candidate_exist = true;
                    }
                    // if (fabs(rotate_speed) <= max_v)
                    // {
                    //     (*iter).update((*fan), src.timestamp);
                    //     (*iter).rotate_speed = rotate_speed;
                    //     break;
                    // }
                }
                if (is_best_candidate_exist)
                {
                    (*best_candidate).update((*fan), src.timestamp);
                    (*best_candidate).rotate_speed = min_v;
                }
                else
                {
                    FanTracker fan_tracker((*fan), src.timestamp);
                    trackers_tmp.push_back(fan_tracker);
                }
            }
        }
        for (auto new_tracker : trackers_tmp)
            trackers_.push_back(new_tracker);

        // 检查待激活扇叶是否存在
        Fan target;
        bool is_target_exists = chooseTarget(fans_, target);
        // 若不存在待击打扇叶则返回false
        if (!is_target_exists)
        {
            if(debug_param_.show_all_fans)
            {
                showFans(src);
            }

            lost_cnt_++;
            is_last_target_exists_ = false;

            RCLCPP_WARN(logger_, "No active target...");
            return false;
        }

        int avail_tracker_cnt = 0;
        double rotate_speed_sum = 0;
        double mean_rotate_speed = 0;
        Eigen::Vector3d r_center_sum = {0, 0, 0};
        Eigen::Vector3d mean_r_center = {0, 0, 0};

        // 计算平均转速与平均R字中心坐标
        for(auto tracker : trackers_)
        {
            if (tracker.is_last_fan_exists && tracker.last_timestamp == src.timestamp)
            {
                rotate_speed_sum += tracker.rotate_speed;
                r_center_sum += tracker.last_fan.centerR3d_world;
                avail_tracker_cnt++;
            }
        }

        // 若不存在可用的扇叶则返回false
        if (avail_tracker_cnt == 0)
        {
            lost_cnt_++;
            return false;
        }

        mean_rotate_speed = rotate_speed_sum / avail_tracker_cnt;
        mean_r_center = r_center_sum / avail_tracker_cnt;
        auto r_center_cam = coordsolver_.worldToCam(target.centerR3d_world, rmat_imu_);
        auto center2d_src = coordsolver_.reproject(r_center_cam);
        auto angle = coordsolver_.getAngle(target.armor3d_cam, rmat_imu_);

        target_info.rotate_speed = mean_rotate_speed;
        target_info.r_center = mean_r_center;

        RCLCPP_INFO(logger_, "Target mean_rotate_speed: %lf mean_r_center: {x:%lf y:%lf z:%lf}",
            mean_rotate_speed, mean_r_center[0], mean_r_center[1], mean_r_center[2]);

        // 判断扇叶是否发生切换
        bool is_switched = false;
        double delta_t = (src.timestamp - last_timestamp_);
        auto relative_rmat = last_fan_.rmat.transpose() * target.rmat;
        auto angle_axisd = Eigen::AngleAxisd(relative_rmat);

        double rotate_spd = (angle_axisd.angle() / delta_t * 1e9);
        if(abs(rotate_spd) > buff_param_.max_v)
            is_switched = true;
        target_info.target_switched = is_switched;
        RCLCPP_INFO(logger_, "Target is switched: %d", (int)(is_switched));

        lost_cnt_ = 0;
        last_roi_center_ = center2d_src;
        last_timestamp_ = src.timestamp;
        last_fan_ = target;
        is_last_target_exists_ = true;

        if (isnan(angle[0]) || isnan(angle[1]))
            return false;
        
        auto time_detect = steady_clock_.now();
        double dr_full_ns = (time_detect - time_start).nanoseconds();
        double dr_crop_ns = (time_crop - time_start).nanoseconds();
        double dr_infer_ns = (time_infer - time_start).nanoseconds();

        if(debug_param_.show_all_fans)
        {
            showFans(src);
        }

        if(debug_param_.show_fps)
        {
            char ch[10];
            sprintf(ch, "%.2f", (1e9 / dr_full_ns));
            std::string fps_str = ch;
            putText(src.img, fps_str, {10, 25}, FONT_HERSHEY_SIMPLEX, 1, {0,255,0});
        }

        if(debug_param_.prinf_latency)
        {
            //降低输出频率，避免影响帧率
            if ((int)(src.timestamp) % 10 == 0)
            {
                RCLCPP_INFO(logger_, "-----------TIME------------");
                RCLCPP_INFO(logger_, "Crop: %lfms", (dr_crop_ns / 1e6));
                RCLCPP_INFO(logger_, "Infer: %lfms", (dr_infer_ns / 1e6));
                RCLCPP_INFO(logger_, "Total: %lfms", (dr_full_ns / 1e6));
            }
        }
        if(debug_param_.print_target_info)
        {
            RCLCPP_INFO(logger_, "-----------INFO------------");
            RCLCPP_INFO(logger_, "Yaw: %lf", angle[0]);
            RCLCPP_INFO(logger_, "Pitch: %lf", angle[1]);
            RCLCPP_INFO(logger_, "Dist: %f m", (float)target.armor3d_cam.norm());
            RCLCPP_INFO(logger_, "Is switched: %d", (int)(is_switched));
        }

        return true;
    }

    void Detector::showFans(TaskData& src)
    {
        for (auto fan : fans_)
        {
            char ch[10];
            sprintf(ch, "%.3f", fan.conf);
            std::string conf_str = ch;
            putText(src.img, conf_str, fan.apex2d[4], FONT_HERSHEY_SIMPLEX, 1, {0, 255, 0}, 2);

            char ch1[10];
            std::string key_str = "";
            if (fan.color == 0)
            {
                sprintf(ch1, "%d", fan.key);
                key_str = ch1;
                putText(src.img, key_str, fan.apex2d[0], FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
            }
            if (fan.color == 1)
            {
                sprintf(ch1, "%d", fan.id);
                key_str = ch1;
                putText(src.img, key_str, fan.apex2d[0], FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
            }
            for(int i = 0; i < 5; i++)
                line(src.img, fan.apex2d[i % 5], fan.apex2d[(i + 1) % 5], Scalar(0,255,0), 1);
            auto fan_armor_center = coordsolver_.reproject(fan.armor3d_cam);
            circle(src.img, fan_armor_center, 4, {0, 0, 255}, 2);
        }
    }

    void Detector::printTargetInfo(int idx)
    {
        switch (idx)
        {
        case 0:
            /* code */
            break;
        default:
            break;
        }
    }

    bool Detector::chooseTarget(std::vector<Fan> &fans_, Fan &target)
    {
        float max_area = 0;
        int target_idx = 0;
        int target_fan_cnt = 0;
        for (auto fan : fans_)
        {
            if (fan.id == 1)
            {
                target = fan;
                target_fan_cnt++;
            }
        }
        if (target_fan_cnt == 1)
            return true;
        else
            return false;
    }

    cv::Point2i Detector::cropImageByROI(cv::Mat& img)
    {
        if (!is_last_target_exists_)
        {
            // 当丢失目标帧数过多或lost_cnt为初值
            if (lost_cnt_ > buff_param_.max_lost_cnt || lost_cnt_ == 0)
            {
                return Point2i(0,0);
            }
        }

        // 若目标大小大于阈值
        if ((last_target_area_ / img.size().area()) > buff_param_.no_crop_thres)
        {
            return Point2i(0,0);
        }
        
        // 处理X越界
        if (last_roi_center_.x <= input_size_.width / 2)
            last_roi_center_.x = input_size_.width / 2;
        else if (last_roi_center_.x > (img.size().width - input_size_.width / 2))
            last_roi_center_.x = img.size().width - input_size_.width / 2;
        // 处理Y越界
        if (last_roi_center_.y <= input_size_.height / 2)
            last_roi_center_.y = input_size_.height / 2;
        else if (last_roi_center_.y > (img.size().height - input_size_.height / 2))
            last_roi_center_.y = img.size().height - input_size_.height / 2;

        // 左上角顶点
        auto offset = last_roi_center_ - Point2i(input_size_.width / 2, input_size_.height / 2);
        Rect roi_rect = Rect(offset, input_size_);
        img(roi_rect).copyTo(img);

        return offset;
    }

    void Detector::setDetectorParam(const double& param, int idx)
    {
        switch (idx)
        {
        case 1:
            buff_param_.fan_length = param;
            break;
        case 2:
            buff_param_.max_delta_t = param;
            break;
        case 3:
            buff_param_.max_lost_cnt = param;
            break;
        case 4:
            buff_param_.max_v = param;
            break;
        case 5:
            buff_param_.no_crop_thres = param;
            break;
        default:
            break;
        }
    }

    void Detector::setDebugParam(const bool& param, int idx)
    {
        switch (idx)
        {
        case 1:
            debug_param_.assist_label = param;
            break;
        case 2:
            debug_param_.detect_red = param;
            break;
        case 3:
            debug_param_.prinf_latency = param;
            break;
        case 4:
            debug_param_.print_target_info = param;
            break;
        case 5:
            debug_param_.show_all_fans = param;
            break;
        case 6:
            debug_param_.show_fps = param;
            break;
        case 7:
            debug_param_.using_imu = param;
            break;
        case 8:
            debug_param_.using_roi = param;
            break;
        default:
            break;
        }
    }

} // namespace buff_detector