cmake_minimum_required(VERSION 3.16)

# Components live in ./components (hw_hal, kernel, ui) and ./main.
# Layering is enforced by each component's REQUIRES list and by
# tools/check_layers.py (run from the VS Code task "Check layering").
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3_touchmap)
