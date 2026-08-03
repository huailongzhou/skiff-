# 核心逻辑单元测试:只依赖纯头文件的 skiff 核心库,无需 LVGL/SDL 后端
add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE skiff)
add_test(NAME core COMMAND test_core)
