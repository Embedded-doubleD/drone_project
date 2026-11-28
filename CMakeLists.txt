# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(drone_project)

target_sources(app PRIVATE
    src/main.c
    src/app/events.c
    src/app/fsm.c
    src/sensors/ldr_service.c
    src/sensors/nmea.c
    src/sensors/gps_service.c
    src/output/led_service.c
    src/output/output_service.c
    src/input/button_service.c
    src/telemetry/telemetry_data.c
    src/telemetry/simu_frame.c
    src/telemetry/telemetry_report.c
    src/sensors/imu/imu_service.c
    src/sensors/imu/imu_driver.c
    src/sensors/imu/imu_interrupt.c
    src/sensors/imu/imu_calibration.c
    src/sensors/imu/imu_processor.c
    src/sensors/imu/imu_recovery.c
    src/sensors/temp_hum_service.c
    src/processing/attitude.c
    src/processing/statistics.c
    src/processing/tracker_status.c
    src/processing/motion.c
    src/processing/advanced_stats.c
)

target_include_directories(app PRIVATE
    src
    src/app
    src/sensors
    src/output
    src/input
    src/telemetry
    src/processing
)

target_link_libraries(app PRIVATE m)
