#include <ros/ros.h>
#include <ros/console.h>
#include <cmath>
#include <iostream>
#include <visualization_msgs/Marker.h>

#include <nav_msgs/OccupancyGrid.h>
#include <nautonomous_mpc_msgs/StageVariable.h>
#include <nautonomous_mpc_msgs/WaypointList.h>
#include <nautonomous_mpc_msgs/Obstacle.h>
#include <geometry_msgs/Point.h>
#include <nautonomous_planning_and_control_using_search/node_tree.h>
#include <Eigen/Dense>

#define INF 1000000
#define PI 3.141592653589793238462643383279502884197169399375105820974
#define sq2 1.414213562373095048801688724209698078569671875376948073176
#define sq5 2.236067977499789696409173668731276235440618359611525724270

using namespace Eigen;

VectorXd NodeNrMatrix(4000000);
MatrixXd CostMatrix(100000,2);	// Colummn 0: cost, Colummn 1: X-pos, Colummn 2: Y-pos

MatrixXf::Index minRow;

ros::Subscriber map_sub;
ros::Subscriber start_sub;
ros::Subscriber goal_sub;
ros::Subscriber obstacle_sub;

ros::Publisher map_pub;
ros::Publisher marker_pub;
ros::Publisher marker_2_pub;

nav_msgs::OccupancyGrid map;
nav_msgs::OccupancyGrid temp_map;
nautonomous_mpc_msgs::StageVariable start_state;
nautonomous_mpc_msgs::StageVariable goal_state;
nautonomous_mpc_msgs::StageVariable waypoint;
nautonomous_mpc_msgs::WaypointList Route;
nautonomous_mpc_msgs::Obstacle Obstacle;

node* starting_node = new node();
node* current_node = new node();
node* new_node = new node();
node* final_node = new node();

float minCost = INF;
float temp_x;
float temp_y;
float temp_theta;

float step_size = 1;

float cost_c;
float cost_i;

float map_width = 0;
float map_height = 0;
float map_center_x = 0;
float map_center_y = 0;
float resolution;

int checks = 0;
float check1 = 0;
float check2 = 0;
float check3 = 0;
float check4 = 0;
float check5 = 0;
float check6 = 0;

std::vector<node>* Network = new std::vector<node>();

geometry_msgs::Point p;
geometry_msgs::Point p1;
geometry_msgs::Point p2;


visualization_msgs::Marker line_list;
visualization_msgs::Marker route_list;

int next_node = 1;
int current_node_nr = 0;
int new_node_nr = 0;
int start_node_nr = 0;
int Nx = 0;
int Ny = 0;
float new_cost = 0;

bool useplot = true;

void get_minimum_node()
{
	minRow = 0;
	minCost = INF;

	for ( int i = 0; i < next_node ; i++)
	{
		//ROS_DEBUG_STREAM( "Node: " << i << " cost: " << CostMatrix(i,0) << "	 " ;
		
		if (CostMatrix(i,0) < minCost)
		{
			minCost = CostMatrix(i,0);
			minRow = i;
		}
	}
	//ROS_DEBUG_STREAM( std::endl;

}

void add_new_node()
{
	ROS_DEBUG_STREAM( "Temp_x: " << temp_x );
	ROS_DEBUG_STREAM( "Temp_y: " << temp_y );
	ROS_DEBUG_STREAM( "map_center_x: " << map_center_x );
	ROS_DEBUG_STREAM( "Nx: " << Nx );
	ROS_DEBUG_STREAM( "map_center_y: " << map_center_y );

	new_node_nr = (temp_x - map_center_x) + Nx * (temp_y - map_center_y);

	ROS_DEBUG_STREAM( "new_node_nr: " << new_node_nr );

	if (useplot)
	{
		p.x = current_node->getX();
		p.y = current_node->getY();
		line_list.points.push_back(p);
		p.x = temp_x;
		p.y = temp_y;
		line_list.points.push_back(p);
	}
	
	if((temp_x < -(map_width*resolution/2)) || (temp_x > (map_width*resolution/2)) || (temp_y < -(map_height*resolution/2)) || (temp_y > (map_height*resolution/2)))
	{	
		new_node->initializeNode(temp_x, temp_y, temp_theta, INF, new_cost,current_node->getNode(), new_node_nr, false);
	}
	else if((int)map.data[(floor((temp_y-map_center_y)/resolution)-1) * map_width + floor((temp_x-map_center_x)/resolution)] > 50)
	{
		new_node->initializeNode(temp_x, temp_y, temp_theta, INF, new_cost,current_node->getNode(), new_node_nr, false);
	}			
	else
	{	
		new_node->initializeNode(temp_x, temp_y, temp_theta, sqrt(pow(temp_x - goal_state.x,2) + pow(temp_y - goal_state.y,2)), new_cost,current_node->getNode(), new_node_nr, false);	
	}

	ROS_DEBUG_STREAM( "Node " << new_node_nr << " is at [" << new_node->getX() << ", " << new_node->getX() << "] at a theta of " << temp_theta << " with a cost of " << new_node->getTotalCost() );

	if (new_node->getTotalCost() >= INF)
	{
		// Do nothing
	}
	else if (NodeNrMatrix(new_node_nr) >= INF)
	{
		Network->push_back(*new_node);
		current_node->addConnectedNode(next_node);
		CostMatrix(next_node,0) = new_node->getTotalCost();
		CostMatrix(next_node,1) = new_node_nr;
		NodeNrMatrix(new_node_nr) = next_node;
		next_node++;

	}
	else if (new_node->getTotalCost() < Network->at(NodeNrMatrix(new_node_nr)).getTotalCost())
	{
		Network->at(NodeNrMatrix(new_node_nr)) = *new_node;
		CostMatrix(next_node,0) = new_node->getTotalCost();
		CostMatrix(next_node,1) = new_node_nr;
	}

}

void calculate_route()
{
	// Clear and initialize
	double begin_check4 = ros::Time::now().toSec();	
	p.z = 0.5;

	ROS_DEBUG_STREAM( "//////////////////START NEW ROUTE//////////////////" );

	new_node_nr = (start_state.x - map_center_x) + Nx * (start_state.y - map_center_y);

	start_node_nr = new_node_nr;

	starting_node->initializeNode(start_state.x, start_state.y, start_state.theta, sqrt(pow(start_state.x - goal_state.x,2) + pow(start_state.y - goal_state.y,2)), 0.0, 0, new_node_nr, false);

	Network->push_back(*starting_node);

	current_node = starting_node;

	CostMatrix(0,0) = 0.0;
	CostMatrix(0,1) = new_node_nr;
	NodeNrMatrix(new_node_nr) = 0.0;

	check4 += ros::Time::now().toSec() - begin_check4;

	ROS_DEBUG_STREAM( "//////////////////Generate//////////////////" );
	double begin_check5 = ros::Time::now().toSec();	
	while (current_node->getDistToFinish() > step_size )
	{
		double begin_check2 = ros::Time::now().toSec();	
	
		if ((std::abs(fmod(current_node->getTheta(),0.5*PI)) < 0.01) || (std::abs(fmod(current_node->getTheta(),0.5*PI) - 0.5 * PI) < 0.01) || (std::abs(fmod(current_node->getTheta(),0.5*PI) + 0.5 * PI) < 0.01))
		{
			// Node right forward	
			temp_theta = current_node->getTheta() - 0.25 * PI;
			temp_x = current_node->getX() + step_size * sq2 * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sq2 * sin(temp_theta);
			new_cost = current_node->getCost() + step_size * sq2;
			
			add_new_node();

			// Node forwards
		
			temp_theta = current_node->getTheta();
			temp_x = current_node->getX() + step_size * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sin(temp_theta);
			new_cost = current_node->getCost() + step_size;
	
			add_new_node();

			// Node left forward
		
			temp_theta = current_node->getTheta() +  0.25 * PI;
			temp_x = current_node->getX() + step_size * sq2 * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sq2 * sin(temp_theta);
			new_cost = current_node->getCost() + step_size * sq2;
			
			add_new_node();
		}

		else if ((std::abs(fmod(current_node->getTheta(),0.5*PI) - 0.25*PI) < 0.01) || (std::abs(fmod(current_node->getTheta(),0.5*PI) + 0.25*PI) < 0.01))
		{
			// Node right forward	
			temp_theta = current_node->getTheta() -  0.25 * PI;
			temp_x = current_node->getX() + step_size * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sin(temp_theta);
			new_cost = current_node->getCost() + step_size;
			
			add_new_node();

			// Node forwards
			temp_theta = current_node->getTheta();
			temp_x = current_node->getX() + step_size * sq2 * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sq2 * sin(temp_theta);
			new_cost = current_node->getCost() + step_size * sq2;
			
			add_new_node();

			// Node left forward
			temp_theta = current_node->getTheta() +  0.25 * PI;
			temp_x = current_node->getX() + step_size * cos(temp_theta);
			temp_y = current_node->getY() + step_size * sin(temp_theta);
			new_cost = current_node->getCost() + step_size;
			
			add_new_node();
		}

		else
		{
			ROS_DEBUG_STREAM( "Angle: " << fmod(current_node->getTheta(),0.5*PI) );
			break;
		}

		check2 += ros::Time::now().toSec() - begin_check2;
		ROS_DEBUG_STREAM( "Elapsed time of check 2 is: " << check2 );

		double begin_check3 = ros::Time::now().toSec();	

		CostMatrix(NodeNrMatrix(current_node->getNode()),0) = INF;
	
		ROS_DEBUG_STREAM( "Check minimum node" );
		get_minimum_node();

		ROS_DEBUG_STREAM( "Minimum node is: " << minRow << " at " << Network->at(minRow).getNode() );
		current_node = &Network->at(minRow);

		check3 += ros::Time::now().toSec() - begin_check3;
		ROS_DEBUG_STREAM( "Elapsed time of check 3 is: " << check3 );

		double begin_check6 = ros::Time::now().toSec();	
		if (useplot)
		{
			marker_pub.publish(line_list);		
		}
		check6 += ros::Time::now().toSec() - begin_check6;
		ROS_DEBUG_STREAM( "Elapsed time of check 6 is: " << check6 );

		checks++;
		if (checks > 10000)
		{
			break;
		}
		//ros::Duration(0.01).sleep();
	}

	ROS_DEBUG_STREAM( "Elapsed time of initialization: " << check4 );


	ROS_DEBUG_STREAM( "//////////////////Track back//////////////////" );
	p.z = 1.0;
	while (not(current_node->getNode() == start_node_nr))
	{
		p.x = current_node->getX();
		p.y = current_node->getY();
      		route_list.points.push_back(p);
		current_node = &Network->at(NodeNrMatrix(current_node->getPreviousNode()));
		p.x = current_node->getX();
		p.y = current_node->getY();
      		route_list.points.push_back(p);
		marker_2_pub.publish(route_list);
	}

	check5 += ros::Time::now().toSec() - begin_check5;
	ROS_DEBUG_STREAM( "Elapsed time of route calculation: " << check5 );
}

void generate_route()
{
	while (not(current_node->getNode() == 0))
	{
		waypoint.x = current_node->getX();
		waypoint.y = current_node->getY();
		Route.stages.push_back(waypoint);
		current_node = &Network->at(current_node->getPreviousNode());
	}
}

void start_cb (const nautonomous_mpc_msgs::StageVariable::ConstPtr& state_msg)
{

	start_state = *state_msg;
	ROS_DEBUG_STREAM( "Start: " << start_state );

	ROS_DEBUG_STREAM( "End: " << goal_state );
	ros::Duration(5).sleep();

	double begin = ros::Time::now().toSec();
	
	calculate_route();

	double end = ros::Time::now().toSec();	
	ROS_INFO_STREAM( "Elapsed time is: " << end-begin );
	ROS_DEBUG_STREAM( "Nodes checked: " << next_node );

	Network->clear();
	CostMatrix = MatrixXd::Ones(100000,2) * INF;
	NodeNrMatrix = VectorXd::Ones(4000000) * INF;
	Network->reserve(100000);

	next_node = 1;
	current_node_nr = 0;
	new_node_nr = 0;
	start_node_nr = 0;
	new_cost = 0;
	checks = 0;
}

void goal_cb (const nautonomous_mpc_msgs::StageVariable::ConstPtr& state_msg)
{
	goal_state = *state_msg;
	final_node->initializeNode(goal_state.x, goal_state.y, goal_state.theta, 0.0, INF, 0, 0, false);
}

void map_cb (const nav_msgs::OccupancyGrid::ConstPtr& map_msg)
{
	ROS_DEBUG_STREAM( "map received" );
	map = *map_msg;
	temp_map = map;
	
	ROS_DEBUG_STREAM( "Data length is: " << map.data.size() );
	map_width = (float)map.info.width;
	map_height = (float)map.info.height;

	Nx = ceil(map_width / step_size);
	Ny = ceil(map_height / step_size);
 
	map_center_x = (float)map.info.origin.position.x;
	map_center_y = (float)map.info.origin.position.y;

	resolution = (float)map.info.resolution;
	ROS_DEBUG_STREAM( "Map center: " << map_center_x << ", " << map_center_y );
	ROS_DEBUG_STREAM( "Map size: " << map_width << " x " << map_height );
}

void obstacle_cb (const nautonomous_mpc_msgs::Obstacle::ConstPtr& obstacle_msg)
{
	Obstacle = *obstacle_msg;
	map = temp_map;
	for (float i = -Obstacle.major_semiaxis; i < Obstacle.major_semiaxis; i+= resolution)
	{
		for (float j = -Obstacle.minor_semiaxis; j < Obstacle.minor_semiaxis; j+= resolution)
		{
			temp_x = Obstacle.state.pose.position.x + i;
			temp_y = Obstacle.state.pose.position.y + j;
			map.data[(floor((temp_y-map_center_y)/resolution)-1) * map_width + floor((temp_x-map_center_x)/resolution)] = 100;
		}		
	}

	map_pub.publish(map);

	ROS_DEBUG_STREAM( "Obstacle map published" );
}

int main (int argc, char** argv)
{
	ros::init (argc, argv,"A_star_tree_path_finding_opt");
	ros::NodeHandle nh("");
	ros::NodeHandle nh_private("~");
	
	map_sub = 	nh.subscribe<nav_msgs::OccupancyGrid>("/map",10,map_cb);
	start_sub = 	nh.subscribe<nautonomous_mpc_msgs::StageVariable>("/mission_coordinator/start",10,start_cb);
	goal_sub = 	nh.subscribe<nautonomous_mpc_msgs::StageVariable>("/mission_coordinator/goal",10,goal_cb);
	obstacle_sub = 	nh.subscribe<nautonomous_mpc_msgs::Obstacle>("/mission_coordinator/obstacle",10,obstacle_cb);

	map_pub = 	nh.advertise<nav_msgs::OccupancyGrid>("/map_tree_opt",10);
	marker_pub = nh_private.advertise<visualization_msgs::Marker>("visualization_marker", 10);
	marker_2_pub = nh_private.advertise<visualization_msgs::Marker>("visualization_marker_route", 10);

	Network->reserve(100000);

	line_list.header.frame_id = "/map";
	line_list.header.stamp = ros::Time::now();
	line_list.ns = "points_and_lines";
	line_list.action = visualization_msgs::Marker::ADD;
	line_list.pose.orientation.w = 1.0;

	line_list.id = 0;
	line_list.type = visualization_msgs::Marker::LINE_LIST;
	line_list.scale.x = 0.2;
	line_list.color.r = 1.0;
	line_list.color.a = 1.0;

	route_list.header.frame_id = "/map";
	route_list.header.stamp = ros::Time::now();
	route_list.ns = "points_and_lines";
	route_list.action = visualization_msgs::Marker::ADD;
	route_list.pose.orientation.w = 1.0;

	route_list.id = 0;
	route_list.type = visualization_msgs::Marker::LINE_LIST;
	route_list.scale.x = 1.0;
	route_list.color.g = 1.0;
	route_list.color.a = 1.0;

	Network->clear();
	CostMatrix = MatrixXd::Ones(100000,2) * INF;
	NodeNrMatrix = VectorXd::Ones(4000000) * INF;
	next_node = 1;

	ros::spin();	
}
