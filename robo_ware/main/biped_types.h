#ifndef MPU_H
#define MPU_H

typedef struct {
    float roll, pitch, yaw;     // For 3-Axis rotation
    float w, x, y, z;           // For quaternions 
    float linear_acc_x, linear_acc_y, linear_acc_z; // Raw accel
    float rot_acc_x, rot_acc_y, rot_acc_z;          // Raw gyro
} mpu_data_t;

#endif
