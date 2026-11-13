# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(drone_project)

target_sources(app PRIVATE
    src/main.c
    src/app/events.c
    src/app/fsm.c
    src/sensors/ldr_service.c
    src/output/led.c
    src/input/button_service.c
)

target_include_directories(app PRIVATE
    src
    src/app
    src/sensors
    src/output
    src/input
)
