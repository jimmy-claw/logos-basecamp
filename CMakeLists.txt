
# Shared skin config library (linked by both app and plugin)
add_library(skin_config STATIC shared/SkinConfig.cpp)
target_compile_features(skin_config PUBLIC cxx_std_17)
target_include_directories(skin_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_link_libraries(skin_config PUBLIC Qt6::Core)
