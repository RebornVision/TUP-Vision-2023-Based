/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-12-10 21:50:43
 * @LastEditTime: 2023-01-15 00:33:54
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/buff/buff_processor/test/src/predictor/predictor.cpp
 */
#include "../../include/predictor/predictor.hpp"

namespace buff_processor
{
    BuffPredictor::BuffPredictor()
    : logger_(rclcpp::get_logger("buff_predictor"))
    {
        is_params_confirmed = false;
        last_mode = mode = -1;
        angle_offset_ = 0.0;
        sign_ = 0;
        is_switched_ = false;
        
        params[0] = 0;
        params[1] = 0; 
        params[2] = 0; 
        params[3] = 0;

        try
        {
            YAML::Node config = YAML::LoadFile(predictor_param_.pf_path);
            pf_param_loader.initParam(config, "buff");
        }
        catch(const std::exception& e)
        {
            RCLCPP_ERROR(logger_, "Error while initializing pf param: %s", e.what());
        }
    }

    BuffPredictor::~BuffPredictor()
    {
    }

    bool BuffPredictor::curveFitting(BuffMsg& buff_msg)
    {
        TargetInfo target = 
        {
            buff_msg.target_switched, 
            buff_msg.angle,
            0.0,
            buff_msg.delta_angle,
            buff_msg.angle_offset,
            buff_msg.timestamp
        };

        if(mode != last_mode)
        {   //模式切换重置预测
            int local_mode = mode;
            last_mode = local_mode;
            angle_offset_ = 0.0;
            sign_ = 0;
            is_switched_ = false;
            history_info.clear();
            pf.initParam(pf_param_loader);
            is_params_confirmed = false;
        }

        if (history_info.size() == 0 || (target.timestamp - history_info.front().timestamp) / 1e6 >= predictor_param_.max_timespan)
        {   //当时间跨度过长视作目标已更新，需清空历史信息队列
            history_info.clear();
            sign_ = 0;
            angle_offset_ = 0.0;
            is_switched_ = false;
            base_angle_ = target.abs_angle;
            target.relative_angle = 0.0;
            history_info.push_back(target);
            params[0] = 0.01;
            params[1] = 0.01; 
            params[2] = 0.01; 
            params[3] = 0.01;
            pf.initParam(pf_param_loader);
            last_target = target;
            is_params_confirmed = false;
            return false;
        }

        //输入数据前进行滤波
        auto is_ready = pf.is_ready;
        Eigen::VectorXd measure(1);
        measure << buff_msg.delta_angle;
        pf.update(measure);
        
        if(is_ready)
        {
            auto predict = pf.predict();
            target.delta_angle = predict[0];
        }

        if(history_info.size() < 50)
        {
            if(target.angle_offset < 0.085)
            {
                target.relative_angle = history_info.back().relative_angle + abs(target.delta_angle);
                history_info.push_back(target);
            }
            else
            {
                //扇叶发生切换，此帧的旋转角度可由插值或已激活能量机关的旋转角度补充
            }
            last_target = target;
            return false;
        }
        else
        {
            if(target.angle_offset < 0.085)
            {
                history_info.pop_front();
                double bAngle = history_info[0].delta_angle;
                for(auto &target_info : history_info)
                    target_info.relative_angle -= abs(bAngle);
                target.relative_angle = history_info.back().relative_angle + abs(target.delta_angle);
                history_info.push_back(target);
            }
            else
            {
                //TODO: 
            }
        }

        // if(history_info.size() == 1)
        // {
        //     double relative_angle = 0.0;
        //     if(target.angle_offset < 0.085)
        //     {
        //         relative_angle = abs(target.abs_angle - base_angle_);
        //         target.relative_angle = relative_angle;
        //         history_info.push_back(target);
        //     }
        //     else
        //     {
        //         target.angle_offset = 0.0;
        //         history_info.pop_front();
        //         base_angle_ = target.abs_angle;
        //         target.relative_angle = 0.0;
        //         history_info.push_back(target);
        //     }
        //     last_target = target;
        //     return false;
        // }
        // else if(history_info.size() < 50)
        // {
        //     double relative_angle = 0.0;
        //     if(target.angle_offset < 0.085)
        //     {
        //         relative_angle = abs(target.abs_angle - base_angle_);
        //         target.relative_angle = relative_angle;
        //         history_info.push_back(target);
        //     }
        //     else
        //     {
        //         RCLCPP_INFO(logger_, "target_offset: %lf", target.angle_offset);
        //         base_angle_ += target.angle_offset;
        //         relative_angle = abs(target.abs_angle - base_angle_);
        //         target.relative_angle = relative_angle;
        //         // history_info.push_back(target);
        //     }
        //     last_target = target;
        //     return false;
        // }
        // else
        // {
        //     double relative_angle = 0.0;
        //     if(target.angle_offset < 0.085)
        //     {
        //         RCLCPP_INFO(logger_, "abs_pre: %lf", base_angle_);
        //         double offset_ang = base_angle_ - history_info[0].abs_angle;    
        //         history_info.pop_front();
        //         base_angle_ = history_info[0].abs_angle + offset_ang;
        //         RCLCPP_INFO(logger_, "abs_back: %lf", base_angle_);

        //         double ang = history_info[0].relative_angle;
        //         for(auto &target_info : history_info)
        //             target_info.relative_angle -= ang;
        //         relative_angle = abs(target.abs_angle - base_angle_);
        //         target.relative_angle = relative_angle;
        //         history_info.push_back(target);
        //     }
        //     else
        //     {
        //         RCLCPP_INFO(logger_, "target_offset: %lf", target.angle_offset);
        //         double offset_ang = base_angle_ - history_info[0].abs_angle;    
        //         history_info.pop_front();
        //         base_angle_ = history_info[0].abs_angle + offset_ang + target.angle_offset;
        //         double ang = history_info[0].relative_angle;
        //         for(auto &target_info : history_info)
        //             target_info.relative_angle -= ang;
        //         relative_angle = abs(target.abs_angle - base_angle_);
        //         target.relative_angle = relative_angle;
        //         history_info.push_back(target);
        //     }
        // }

        double rotate_speed_sum = 0.0;
        double rotate_speed_ave = 0.0;
        double delta_angle_sum = 0.0;
        // RCLCPP_INFO(logger_, "base_angle: %lf", base_angle_);
        // double base_time = history_info[0].timestamp;
        auto origin_target = history_info[0];
        for(auto target_info : history_info)
        {
            delta_angle_sum += target_info.delta_angle;
            double dAngle = (target_info.relative_angle - origin_target.relative_angle);
            if((target_info.timestamp - origin_target.timestamp) != 0)
                rotate_speed_sum += (dAngle / (target_info.timestamp - origin_target.timestamp) * 1e9);
            origin_target = target_info;
            // delta_angle_sum += (target_info.relative_angle - dAngle);
            // dAngle = target_info.relative_angle;
            // RCLCPP_INFO(logger_, "abs_angle: %lf relative_angle: %lf timestamp: %lf", target_info.abs_angle, target_info.relative_angle, (target_info.timestamp - base_time) / 1e9);
        }
        rotate_speed_ave = rotate_speed_sum / (((int)history_info.size())-1);
        sign_ = delta_angle_sum / abs(delta_angle_sum);

        //曲线拟合
        if(mode == 0)
        {   //小符，计算平均角度差
            params[3] = rotate_speed_ave;  //TODO:小能量机关转速10RPM
            is_params_confirmed = true;
            RCLCPP_INFO(logger_, "Average rotate speed: %lf", rotate_speed_ave);
        }
        else if(mode == 1)
        {
            ceres::Problem problem;
            ceres::Solver::Options options;
            ceres::Solver::Summary summary;       // 优化信息
            double params_fitting[4] = {0.1, 0.1, 0.1, 0.1};

            for(auto target_info : history_info)
            {
                problem.AddResidualBlock
                (
                    new ceres::AutoDiffCostFunction<CurveFittingCost, 1, 4>
                    (
                        new CurveFittingCost(target_info.relative_angle, target_info.timestamp / 1e9)
                    ),
                    new ceres::CauchyLoss(0.5),
                    params_fitting
                );
            }

            //设置上下限
            problem.SetParameterLowerBound(params_fitting, 0, 0.7); //a(0.780~1.045)
            problem.SetParameterUpperBound(params_fitting, 0, 1.2); 
            problem.SetParameterLowerBound(params_fitting, 1, 1.6); //w(1.884~2.000)
            problem.SetParameterUpperBound(params_fitting, 1, 2.2);
            problem.SetParameterLowerBound(params_fitting, 2, -CV_PI); //θ
            problem.SetParameterUpperBound(params_fitting, 2, CV_PI);
            problem.SetParameterLowerBound(params_fitting, 3, 0.8); //b=2.090-a
            problem.SetParameterUpperBound(params_fitting, 3, 1.5);

            //参数求解
            ceres::Solve(options, &problem, &summary);
            
            //计算拟合后曲线的RMSE指标
            mutex_.lock();
            memcpy(params, params_fitting, sizeof(params));
            is_params_confirmed = true;
            for (auto param : params)
                cout << param << " ";
            std::cout << std::endl;
            mutex_.unlock();
        }   
        else
        {
            mutex_.lock();
            is_params_confirmed = false;
            mutex_.unlock();
            
            return false;
        }
        return true;
    }

    bool BuffPredictor::predict(BuffMsg buff_msg, double dist, double &result)
    {
        double delay = (mode == 1 ? predictor_param_.delay_big : predictor_param_.delay_small);
        float delta_time_estimate = ((double)dist / predictor_param_.bullet_speed) * 1e3 + delay;
        
        if(is_params_confirmed)
        {
            if(mode == 0)
            {
                mutex_.lock();
                result = sign_ * (params[3] * delta_time_estimate / 1e3);
                mutex_.unlock();
            }
            else if(mode == 1)
            {
                mutex_.lock();
                float timespan = history_info.back().timestamp / 1e6;
                float time_estimate = delta_time_estimate + timespan;
                double pre_angle = calPreAngle(params, (time_estimate / 1e3));
                result = sign_ * (pre_angle - history_info.back().relative_angle);
                mutex_.unlock();
                if(result < 0.0)
                    return false;
            }
        }
        else
            return false;
        return true;
    }

    double BuffPredictor::calPreAngle(double* params, double timestamp)
    {
        double pre_angle = -(params[0] / params[1]) * ceres::cos(params[1] * timestamp + params[2]) + params[3] * timestamp + (params[0] / params[1]) * ceres::cos(params[2]);
        return std::move(pre_angle);
    }

    // /**
    //  * @brief 预测
    //  * @param speed 旋转速度
    //  * @param dist 距离
    //  * @param timestamp 时间戳
    //  * @param result 结果输出
    //  * @return 是否预测成功
    // */
    // bool BuffPredictor::predict(double speed, double dist, double timestamp, double &result)
    // {
    //     std::cout << 1 << std::endl;

    //     // TargetInfo target = {speed, dist, timestamp};
    //     TargetInfo target = {};
    //     if (mode != last_mode)
    //     {
    //         last_mode = mode;
    //         history_info.clear();
    //         pf.initParam(pf_param_loader);
    //         is_params_confirmed = false;
    //     }

    //     if (history_info.size() == 0 || (target.timestamp - history_info.front().timestamp) / 1e6 >= predictor_param_.max_timespan)
    //     {   //当时间跨度过长视作目标已更新，需清空历史信息队列
    //         history_info.clear();
    //         history_info.push_back(target);
    //         params[0] = 0;
    //         params[1] = 0; 
    //         params[2] = 0; 
    //         params[3] = 0;
    //         pf.initParam(pf_param_loader);
    //         last_target = target;
    //         is_params_confirmed = false;
    //         return false;
    //     }

    //     std::cout << 2 << std::endl;

    //     //输入数据前进行滤波
    //     auto is_ready = pf.is_ready;
    //     Eigen::VectorXd measure(1);
    //     measure << speed;
    //     pf.update(measure);

    //     if (is_ready)
    //     {
    //         auto predict = pf.predict();
    //         target.speed = predict[0];
    //     }

    //     int deque_len = 0;
    //     if (mode == 0)
    //     {
    //         deque_len = predictor_param_.history_deque_len_uniform;
    //         std::cout << "lens:" << deque_len << std::endl;
    //     }
    //     else if (mode == 1)
    //     {
    //         if (!is_params_confirmed)
    //             deque_len = predictor_param_.history_deque_len_cos;
    //         else
    //             deque_len = predictor_param_.history_deque_len_phase;
    //     }
    //     if ((int)(history_info.size()) < deque_len)    
    //     {
    //         std::cout << "size:" << (int)(history_info.size()) << std::endl;
    //         history_info.push_back(target);
    //         last_target = target;
    //         return false;
    //     }
    //     else if ((int)(history_info.size()) == deque_len)
    //     {
    //         history_info.pop_front();
    //         history_info.push_back(target);
    //     }
    //     else if ((int)(history_info.size()) > deque_len)
    //     {
    //         while((int)(history_info.size()) >= deque_len)
    //             history_info.pop_front();
    //         history_info.push_back(target);
    //     }

    //     std::cout << 3 << std::endl;

    //     // 计算旋转方向
    //     double rotate_speed_sum = 0;
    //     int rotate_sign = 0;
    //     for (auto target_info : history_info)
    //         rotate_speed_sum += target_info.speed;
    //     auto mean_velocity = rotate_speed_sum / history_info.size();

    //     if (mode == 0)
    //     {   //TODO:小符模式不需要额外计算,也可增加判断，小符模式给定恒定转速进行击打
    //         params[3] = mean_velocity;
    //     }
    //     else if (mode == 1)
    //     {   //若为大符
    //         //拟合函数: f(t) = a * sin(ω * t + θ) + b， 其中a， ω， θ需要拟合.
    //         //参数未确定时拟合a， ω， θ
    //         if (!is_params_confirmed)
    //         {
    //             ceres::Problem problem;
    //             ceres::Solver::Options options;
    //             ceres::Solver::Summary summary;       // 优化信息
    //             double params_fitting[4] = {1, 1, 1, mean_velocity};

    //             //旋转方向，逆时针为正
    //             if (rotate_speed_sum / fabs(rotate_speed_sum) >= 0)
    //                 rotate_sign = 1;
    //             else
    //                 rotate_sign = -1;

    //             std::cout << "target_speed:"; 
    //             for (auto target_info : history_info)
    //             {
    //                 std::cout << target_info.speed << " ";
    //                 problem.AddResidualBlock (     // 向问题中添加误差项
    //                 // 使用自动求导，模板参数：误差类型，输出维度，输入维度，维数要与前面struct中一致
    //                     new ceres::AutoDiffCostFunction<CURVE_FITTING_COST, 1, 4> ( 
    //                         new CURVE_FITTING_COST (target_info.speed  * rotate_sign, (double)(target_info.timestamp) / 1e9)
    //                     ),
    //                     new ceres::CauchyLoss(0.5),
    //                     params_fitting                 // 待估计参数
    //                 );
    //             }
    //             std::cout << std::endl;

    //             //设置上下限
    //             //FIXME:参数需根据场上大符实际调整
    //             problem.SetParameterLowerBound(params_fitting, 0, 0.7);
    //             problem.SetParameterUpperBound(params_fitting, 0, 1.2);
    //             problem.SetParameterLowerBound(params_fitting, 1, 1.6);
    //             problem.SetParameterUpperBound(params_fitting, 1, 2.2);
    //             problem.SetParameterLowerBound(params_fitting, 2, -CV_PI);
    //             problem.SetParameterUpperBound(params_fitting, 2, CV_PI);
    //             problem.SetParameterLowerBound(params_fitting, 3, 0.5);
    //             problem.SetParameterUpperBound(params_fitting, 3, 2.5);

    //             ceres::Solve(options, &problem, &summary);
    //             double params_tmp[4] = {params_fitting[0] * rotate_sign, params_fitting[1], params_fitting[2], params_fitting[3] * rotate_sign};
    //             auto rmse = evalRMSE(params_tmp);
    //             if (rmse > predictor_param_.max_rmse)
    //                 return false;
    //             else
    //             {
    //                 params[0] = params_fitting[0] * rotate_sign;
    //                 params[1] = params_fitting[1];
    //                 params[2] = params_fitting[2];
    //                 params[3] = params_fitting[3] * rotate_sign;
    //                 is_params_confirmed = true;
    //             }
    //         }
    //         else
    //         {   //参数确定时拟合θ
    //             ceres::Problem problem;
    //             ceres::Solver::Options options;
    //             ceres::Solver::Summary summary; // 优化信息
    //             double phase;

    //             for (auto target_info : history_info)
    //             {
    //                 problem.AddResidualBlock( // 向问题中添加误差项
    //                 // 使用自动求导，模板参数：误差类型，输出维度，输入维度，维数要与前面struct中一致
    //                     new ceres::AutoDiffCostFunction<CURVE_FITTING_COST_PHASE, 1, 1> 
    //                     ( 
    //                         new CURVE_FITTING_COST_PHASE ((target_info.speed - params[3]) * rotate_sign, 
    //                         (float)((target_info.timestamp) / 1e9),
    //                         params[0], 
    //                         params[1], 
    //                         params[3])
    //                     ),
    //                     new ceres::CauchyLoss(1e1),
    //                     &phase // 待估计参数
    //                 );
    //             }

    //             //设置上下限
    //             problem.SetParameterUpperBound(&phase, 0, CV_PI);
    //             problem.SetParameterLowerBound(&phase, 0, -CV_PI);

    //             ceres::Solve(options, &problem, &summary);
    //             double params_new[4] = {params[0], params[1], phase, params[3]};
    //             auto old_rmse = evalRMSE(params);
    //             auto new_rmse = evalRMSE(params_new);
    //             if (new_rmse < old_rmse)
    //             {   
    //                 params[2] = phase;
    //             }
    //         }
    //     }

    //     for (auto param : params)
    //         cout << param << " ";
    //     std::cout << std::endl;

    //     int delay = (mode == 1 ? predictor_param_.delay_big : predictor_param_.delay_small);
    //     float delta_time_estimate = ((double)dist / predictor_param_.bullet_speed) * 1e3 + delay;
    //     // cout<<"ETA:"<<delta_time_estimate<<endl;
    //     float timespan = history_info.back().timestamp / 1e6;
    //     // delta_time_estimate = 0;
    //     float time_estimate = delta_time_estimate + timespan;
    //     // cout<<delta_time_estimate<<endl;     
    //     std::cout << 4 << std::endl;

    //     result = calcAimingAngleOffset(params, timespan / 1e3, time_estimate / 1e3, mode);
    //     std::cout << 5 << std::endl;
    //     last_target = target;
        
    //     return true;
    // }

    // /**
    //  * @brief 计算角度提前量
    //  * @param params 拟合方程参数
    //  * @param t0 积分下限
    //  * @param t1 积分上限
    //  * @param mode 模式
    //  * @return 角度提前量(rad)
    // */
    // double BuffPredictor::calcAimingAngleOffset(double params[4], double t0, double t1 , int mode)
    // {
    //     auto a = params[0];
    //     auto omega = params[1];
    //     auto theta = params[2];
    //     auto b = params[3]; 
    //     double theta1;
    //     double theta0;
       
    //     //f(t) = a * sin(ω * t + θ) + b
    //     //对目标函数进行积分
    //     if (mode == 0)//适用于小符模式
    //     {
    //         theta0 = b * t0;
    //         theta1 = b * t1;
    //     }
    //     else
    //     {
    //         theta0 = (b * t0 - (a / omega) * cos(omega * t0 + theta));
    //         theta1 = (b * t1 - (a / omega) * cos(omega * t1 + theta));
    //     }
    //     return theta1 - theta0;
    // }

    // /**
    //  * @brief 滑窗滤波
    //  * 
    //  * @param start_idx 开始位置 
    //  * @return double 滤波结果
    //  */
    // inline double BuffPredictor::shiftWindowFilter(int start_idx=0)
    // {
    //     //TODO:修改传入参数，由start_idx改为max_iter
    //     //计算最大迭代次数
    //     auto max_iter = int(history_info.size() - start_idx) - predictor_param_.window_size + 1;

    //     if (max_iter <= 0 || start_idx < 0)
    //         return history_info.back().speed;
    //     // cout<<start_idx<<":"<<history_info.at(start_idx).speed<<endl;
    //     // cout<<start_idx + 1<<":"<<history_info.at(start_idx + 1).speed<<endl;
    //     // cout<<start_idx + 2<<":"<<history_info.at(start_idx + 2).speed<<endl;
    //     // cout<<start_idx + 3<<":"<<history_info.at(start_idx + 3).speed<<endl;
        
    //     double total_sum = 0;
    //     for (int i = 0; i < max_iter; i++)
    //     {
    //         double sum = 0;
    //         for (int j = 0; j < predictor_param_.window_size; j++)
    //             sum += history_info.at(start_idx + i + j).speed;
    //         total_sum += sum / predictor_param_.window_size;
    //     }
    //     return total_sum / max_iter;
    // }

    /**
     * @brief 设置弹速
     * 
     * @param speed 传入弹速
     * @return true 
     * @return false 
     */
    bool BuffPredictor::setBulletSpeed(double speed)
    {
        predictor_param_.bullet_speed = speed;
        return true;
    }

    // /**
    //  * @brief 计算RMSE指标
    //  * 
    //  * @param params 参数首地址指针
    //  * @return RMSE值 
    //  */
    // double BuffPredictor::evalRMSE(double params[4])
    // {
    //     double rmse_sum = 0;
    //     double rmse = 0;
    //     for (auto target_info : history_info)
    //     {
    //         auto t = (float)(target_info.timestamp) / 1e3;
    //         auto pred = params[0] * sin (params[1] * t + params[2]) + params[3];
    //         auto measure = target_info.speed;
    //         rmse_sum += pow((pred - measure), 2);
    //     }
    //     rmse = sqrt(rmse_sum / history_info.size());
    //     return rmse;
    // }

    // /**
    //  * @brief 计算RMSE指标
    //  * 
    //  * @param params 参数首地址指针
    //  * @return RMSE值 
    //  */
    // double BuffPredictor::evalMAPE(double params[4])
    // {
    //     double mape_sum = 0;
    //     double mape = 0;
    //     for (auto target_info : history_info)
    //     {
    //         auto t = (float)(target_info.timestamp) / 1e3;
    //         auto pred = params[0] * sin (params[1] * t + params[2]) + params[3];
    //         auto measure = target_info.speed;

    //         mape_sum += abs((measure - pred) / measure);
    //     }
    //     mape = mape_sum / history_info.size() * 100;
    //     return mape;
    // }

} //namespace buff_processor