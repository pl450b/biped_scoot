/*
 * Display.c
 *
 *  Created on: 14.08.2017
 *      Author: darek
 *
 *  Edited by Wesley on: 29.12.2024
 *      Added task_mpu_producer
 */
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "MPU6050.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "sdkconfig.h"

#include "mpu6050_dev.h"

#define PIN_SDA 21
#define PIN_CLK 22

Quaternion q;           // [w, x, y, z]         quaternion container
VectorFloat gravity;    // [x, y, z]            gravity vector
VectorInt16 accel;       // [x, y, z]            linear acceleration vector
VectorInt16 gyro;       // [x, y, z]            roational acceleration vector

extern QueueHandle_t mpuQueue;

float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
uint16_t packetSize = 42;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU

void init_I2C(void) {
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = (gpio_num_t)PIN_SDA;
	conf.scl_io_num = (gpio_num_t)PIN_CLK;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 400000;
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
}

void task_display(void* pvParameters){
	MPU6050 mpu = MPU6050();
	mpu.initialize();
	mpu.dmpInitialize();

	// This need to be setup individually
	// mpu.setXGyroOffset(220);
	// mpu.setYGyroOffset(76);
	// mpu.setZGyroOffset(-85);
	// mpu.setZAccelOffset(1788);
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);

	mpu.setDMPEnabled(true);

	while(1){
        mpuIntStatus = mpu.getIntStatus();
        // get current FIFO count
        fifoCount = mpu.getFIFOCount();

        if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
            // reset so we can continue cleanly
            mpu.resetFIFO();

        // otherwise, check for DMP data ready interrupt frequently)
        } else if (mpuIntStatus & 0x02) {
            // wait for correct available data length, should be a VERY short wait
            while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

            // read a packet from FIFO

            mpu.getFIFOBytes(fifoBuffer, packetSize);
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);


            printf("YAW: %3.1f, ", ypr[0] * 180/M_PI);
            printf("PITCH: %3.1f, ", ypr[1] * 180/M_PI);
            printf("ROLL: %3.1f \n", ypr[2] * 180/M_PI);
        }

        //Best result is to match with DMP refresh rate
        // Its last value in components/MPU6050/MPU6050_6Axis_MotionApps20.h file line 310
        // Now its 0x13, which means DMP is refreshed with 10Hz rate
        // vTaskDelay(5/portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}

void task_mpu_to_queue(void* pvParameters) {
    mpu_data_t rx_data;

    MPU6050 mpu = MPU6050();
    mpu.initialize();
    mpu.dmpInitialize();

    // This need to be setup individually
    // mpu.setXGyroOffset(220);
    // mpu.setYGyroOffset(76);
    // mpu.setZGyroOffset(-85);
    // mpu.setZAccelOffset(1788);
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);

    mpu.setDMPEnabled(true);

    while(1) {
        mpuIntStatus = mpu.getIntStatus();
        // get current FIFO count
        fifoCount = mpu.getFIFOCount();

        if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
            // reset so we can continue cleanly
            mpu.resetFIFO();

        // otherwise, check for DMP data ready interrupt frequently)
        } else if (mpuIntStatus & 0x02) {
            // wait for correct available data length, should be a VERY short wait
            while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

            // read a packet from FIFO
            mpu.getFIFOBytes(fifoBuffer, packetSize);
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            mpu.dmpGetAccel(&accel, fifoBuffer);
            mpu.dmpGetGyro(&gyro, fifoBuffer);

            rx_data.roll = ypr[0];
            rx_data.pitch = ypr[1];
            rx_data.yaw = ypr[2];

            rx_data.w = q.w;
            rx_data.x = q.x;
            rx_data.y = q.y;
            rx_data.z = q.z;

            rx_data.linear_acc_x = accel.x;
            rx_data.linear_acc_y = accel.y;
            rx_data.linear_acc_z = accel.z;

            rx_data.rot_acc_x = gyro.x;
            rx_data.rot_acc_y = gyro.y;
            rx_data.rot_acc_z = gyro.z;

            xQueueSend(mpuQueue, &rx_data, portMAX_DELAY); 
        }

        //Best result is to match with DMP refresh rate
        // Its last value in components/MPU6050/MPU6050_6Axis_MotionApps20.h file line 310
        // Now its 0x13, which means DMP is refreshed with 10Hz rate
        vTaskDelay(5/portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
  }
