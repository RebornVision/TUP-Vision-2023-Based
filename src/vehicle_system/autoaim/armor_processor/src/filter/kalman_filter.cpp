/*
 * @Description: This is a ros-based project!
 * @Author: Liu Biao
 * @Date: 2022-10-31 19:20:59
 * @LastEditTime: 2022-11-27 17:35:36
 * @FilePath: /TUP-Vision-2023-Based/src/vehicle_system/autoaim/armor_processor/src/filter/kalman_filter.cpp
 */
#include "../../include/filter/kalman_filter.hpp"

using Eigen::VectorXd;
using Eigen::MatrixXd;

namespace armor_processor
{
    KalmanFilter::KalmanFilter(){}

    KalmanFilter::~KalmanFilter(){}

    void KalmanFilter::Init(int SP, int MP, int CP)
    {
        assert(SP > 0 && MP > 0 && CP > 0);

        this->x_ = Eigen::VectorXd::Zero(SP);
        this->F_ = Eigen::MatrixXd::Identity(SP, SP);
        this->Q_ = Eigen::MatrixXd::Identity(SP, SP);
        this->H_ = Eigen::MatrixXd::Zero(MP, SP);
        this->R_ = Eigen::MatrixXd::Zero(MP, MP);
        this->P_ = Eigen::MatrixXd::Zero(SP, SP);
        this->C_ = Eigen::MatrixXd::Zero(SP, CP);

        this->cp_ = CP;
    }

    void KalmanFilter::Init(MatrixXd& x_in, MatrixXd& P_in, MatrixXd& F_in,
            MatrixXd& H_in, MatrixXd& R_in, MatrixXd& Q_in)
    {
        x_ = x_in;
        P_ = P_in;
        F_ = F_in;
        H_ = H_in;
        R_ = R_in;
        Q_ = Q_in;
    }

    void KalmanFilter::Init(MatrixXd& x_in, MatrixXd& P_in, MatrixXd& F_in,
            MatrixXd& H_in, MatrixXd& R_in, MatrixXd& Q_in, MatrixXd& J_in)
    {
        x_ = x_in;
        P_ = P_in;
        F_ = F_in;
        H_ = H_in;
        R_ = R_in;
        Q_ = Q_in;
        J_ = J_in;
    }

    void KalmanFilter::Predict()
    {
        if(this->cp_ > 0)
        {
            x_ = F_ * x_ + C_ * x_[2];
        }
        else
        {
            x_ = F_ * x_;
        }

        MatrixXd Ft = F_.transpose();
        
        P_ = F_ * P_ * Ft + Q_;
    }

    void KalmanFilter::Predict(const double& dt)
    {
        this->dt = dt;
        updatePrediction();
        this->P_ = this->F_ * this->P_ * this->F_.transpose() + this->Q_;
    }

    void KalmanFilter::Update(const VectorXd& z)
    {
        MatrixXd z_pred = H_ * x_;

        MatrixXd y = z - z_pred;
        
        //卡尔曼增益
        MatrixXd Ht = H_.transpose();
        MatrixXd PHt = P_ * Ht;
        MatrixXd S = H_ * PHt + R_;
        MatrixXd Si = S.inverse();
        MatrixXd K = PHt * Si;

        //update
        x_ = x_ + (K * y);
        int x_size = x_.size();
        MatrixXd I = MatrixXd::Identity(x_size, x_size);
        P_ = (I - K * H_) * P_;
    }

    void KalmanFilter::Update(const Eigen::VectorXd& z, int mp)
    {
        updateMeasurement();

        // 测量值与预测值之间的残差
        Eigen::VectorXd v = z - this->x_;
        
        // 残差的协方差矩阵
        Eigen::MatrixXd S = this->H_ * this->P_ * this->H_.transpose() + this->R_;
        this->S_ = S;

        // 计算模型似然值
        double det = S.determinant();
        this->likelihood_ = exp(-0.5 * v.transpose() * S.inverse() * v) / sqrt(pow(2 * M_PI, mp) * det);

        // 计算卡尔曼增益
        Eigen::MatrixXd K = this->P_ * this->H_.transpose() * S.inverse();

        //更新
        this->x_ = this->x_ + K * v;
        Eigen::MatrixXd I;
        I.setIdentity();
        Eigen::MatrixXd C = (I - K * this->H_);
        this->P_ = C * this->P_ * C.transpose() + K * this->R_ * K.transpose();
    }

    void KalmanFilter::updateOnce(const double& dt, const Eigen::VectorXd* z)
    {
        if(z->isZero())
        {   //若没有观测值传入，则只进行预测
            Predict(dt);
        }
        else
        {   
            Predict(dt);
            Update(*z);
        }
    }

    void KalmanFilter::UpdateJacobians()
    {
        
    }

    void KalmanFilter::UpdateEKF(const MatrixXd& z)
    {
        //TODO:
    }

    double KalmanFilter::getLikelihoodValue() const
    {
        return this->likelihood_;
    }

    // KalmanFilter::Clone()
    // {
    //     return new KalmanFilter(*this);
    // }

} // armor_processor
