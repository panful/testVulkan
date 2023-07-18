set(target_name "01_HelloTriangle")

add_executable(${target_name} "main.cpp")

# 3rdparty
target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/includes)
target_link_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/libs/$<IF:$<CONFIG:Debug>,Debug,Release>/${CMAKE_HOST_SYSTEM_NAME})

# vulkan
target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(${target_name} PRIVATE glfw3 ${Vulkan_LIBRARIES})

install(TARGETS ${target_name} RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders DESTINATION resources)

add_custom_command(TARGET ${target_name}
    PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E
    copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders $<TARGET_FILE_DIR:${target_name}>/resources/shaders
)
