cmake_minimum_required(VERSION 2.8.3)
project(nautonomous_planning_and_control_using_search)

find_package(catkin REQUIRED
roscpp
nautonomous_mpc_msgs
nautonomous_state_msgs
)

catkin_package(
)



include_directories(
	include
	${catkin_INCLUDE_DIRS}
)


# add executable
add_executable(grid_planner src/Astar_grid.cpp)
add_executable(tree_opt src/Astar_tree_search.cpp)
add_executable(tree_opt_closest_with_moving_obstacle_first src/Astar_tree_search_closest_first_with_moving_obstacle.cpp)
add_executable(Jump_point_search src/Jump_point_search.cpp)

add_executable(visualization src/visualization.cpp)
add_executable(mission_control src/mission_control_sim.cpp)

# target link libraries
target_link_libraries(grid_planner ${catkin_LIBRARIES} )
target_link_libraries(tree_opt ${catkin_LIBRARIES} )
target_link_libraries(tree_opt_closest_with_moving_obstacle_first ${catkin_LIBRARIES} )
target_link_libraries(Jump_point_search ${catkin_LIBRARIES} )

target_link_libraries(visualization ${catkin_LIBRARIES} )
target_link_libraries(mission_control ${catkin_LIBRARIES} )

