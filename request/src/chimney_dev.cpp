/*!
 * \file requester.cpp
 *
 * More elaborate description
 */
#include <ros/ros.h>
#include "koptplanner/inspection.h"
#include "shape_msgs/SolidPrimitive.h"
#include <cstdlib>
#include <visualization_msgs/MarkerArray.h>
#include <fstream>
#include <geometry_msgs/PolygonStamped.h>
#include <nav_msgs/Path.h>
#include <std_msgs/Int32.h>
#include <ros/package.h>
#include "tf/tf.h"
void readSTLfile(std::string name, visualization_msgs::MarkerArray* mesh);
int main(int argc, char **argv)
{
  ros::init(argc, argv, "chimney");
  ROS_INFO("Requester is alive");
  if (argc != 1)
  {
    ROS_INFO("usage: plan");
    return 1;
  }
  ros::NodeHandle n;
  ros::Publisher obstacle_pub = n.advertise<visualization_msgs::Marker>("scenario", 1);
  ros::Publisher stl_pub = n.advertise<visualization_msgs::Marker>("stl_mesh", 1);
  ros::ServiceClient client = n.serviceClient<koptplanner::inspection>("inspectionPath");
  ros::Rate r(50.0);
  ros::Rate r2(1.0);
  r2.sleep();
  /* define the bounding box */
  koptplanner::inspection srv;
  srv.request.spaceSize.push_back(50);
  srv.request.spaceSize.push_back(50);
  srv.request.spaceSize.push_back(280);
  srv.request.spaceCenter.push_back(0);
  srv.request.spaceCenter.push_back(0);
  srv.request.spaceCenter.push_back(12);
  geometry_msgs::Pose reqPose;
  /* starting pose */
  reqPose.position.x = 25.0;
  reqPose.position.y = 25.0;
  reqPose.position.z = -55.0;
  tf::Quaternion q = tf::createQuaternionFromRPY(0.0, 0.0, 0.0);
  reqPose.orientation.x = q.x();
  reqPose.orientation.y = q.y();
  reqPose.orientation.z = q.z();
  reqPose.orientation.w = q.w();
  srv.request.requiredPoses.push_back(reqPose);
  /* final pose (remove if no explicit final pose is desired) 
  reqPose.position.x = 15.0;
  reqPose.position.y = 15.0;
  reqPose.position.z = -8.0;
  q = tf::createQuaternionFromRPY(0.0, 0.0, 0.0);
  reqPose.orientation.x = q.x();
  reqPose.orientation.y = q.y();
  reqPose.orientation.z = q.z();
  reqPose.orientation.w = q.w();

  srv.request.requiredPoses.push_back(reqPose);

  /* parameters for the path calculation (such as may change during mission) */
  srv.request.incidenceAngle = M_PI/6.0;
  srv.request.minDist = 4.0;
  srv.request.maxDist = 8.0;
  srv.request.numIterations = 20;
  /* read STL file and publish to rviz */
  visualization_msgs::MarkerArray mesh;
  readSTLfile(ros::package::getPath("request")+"/meshes/chimney.stl", &mesh);
  ROS_INFO("mesh size = %i", (int)mesh.markers[0].points.size() / 3);
  stl_pub.publish(mesh);
  int j = 0;

  srv.request.inspectionArea.resize(228368);

  geometry_msgs::Polygon poly;
  poly.points.resize(3);
  for (geometry_msgs::Point p : mesh.markers[0].points)
  {
    geometry_msgs::Point32 p32;
    p32.x = p.x; p32.y = p.y; p32.z = p.z;
    poly.points[j%3] = p32;
    if((++j) % 3 == 0)  
    {
      srv.request.inspectionArea.push_back(poly);
    }
    ROS_INFO("holding here");
  }
ROS_INFO("call service");
  if (client.call(srv))
  {

    /* writing results of scenario to m-file. use supplied script to visualize */
    std::fstream pathPublication;
    std::string pkgPath = ros::package::getPath("request");
    pathPublication.open((pkgPath+"/visualization/inspectionScenario.m").c_str(), std::ios::out);
    if(!pathPublication.is_open())
    {
      ROS_ERROR("Could not open 'inspectionScenario.m'! Inspection path is not written to file");
      return 1;
    }
    pathPublication << "inspectionPath = [";
    for(std::vector<geometry_msgs::PoseStamped>::iterator it = srv.response.inspectionPath.poses.begin(); it != srv.response.inspectionPath.poses.end(); it++)
    {
      tf::Pose pose;
      tf::poseMsgToTF(it->pose, pose);
      double yaw_angle = tf::getYaw(pose.getRotation());
      pathPublication << it->pose.position.x << ", " << it->pose.position.y << ", " << it->pose.position.z << ", 0, 0, " << yaw_angle << ";\n";
    }
    pathPublication << "];\n";
    pathPublication << "inspectionCost = " << srv.response.cost << ";\n";
    pathPublication << "numObstacles = " << std::min(srv.request.obstacles.size(),srv.request.obstaclesPoses.size()) << ";\n";
    for(int i = 0; i<std::min(srv.request.obstacles.size(),srv.request.obstaclesPoses.size()); i++)
    {
      pathPublication << "obstacle{" << i+1 << "} = [" << srv.request.obstacles[i].dimensions[0] << ", " << srv.request.obstacles[i].dimensions[1] << ", " << srv.request.obstacles[i].dimensions[2] << ";\n";
      pathPublication << srv.request.obstaclesPoses[i].position.x << ", " << srv.request.obstaclesPoses[i].position.y << ", " << srv.request.obstaclesPoses[i].position.z << "];\n";
    }
    pathPublication << "meshX = [";
    j = 0;
    for (geometry_msgs::Point p : mesh.markers[0].points)
      pathPublication << p.x << ((j++)%3==2 ? ";\n" : ", ");
    pathPublication << "];\nmeshY = [";
    j = 0;
    for (geometry_msgs::Point p : mesh.markers[0].points)
      pathPublication << p.y << ((j++)%3==2 ? ";\n" : ", ");
    pathPublication << "];\nmeshZ = [";
    j = 0;
    for (geometry_msgs::Point p : mesh.markers[0].points)
      pathPublication << p.z << ((j++)%3==2 ? ";\n" : ", ");
    pathPublication << "];\n";
  }
  else
  {
    ROS_ERROR("Failed to call service planner");
    return 1;
  }
  return 0;
}
void readSTLfile(std::string name, visualization_msgs::MarkerArray* mesh)
{
  std::fstream f;
  f.open(name.c_str());
  assert(f.is_open());
  int MaxLine = 0;
  char* line;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
  double maxZ = -DBL_MAX;
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double minZ = DBL_MAX;
  assert(line = (char *) malloc(MaxLine = 80));
  f.getline(line, MaxLine);
  if(0 != strcmp(strtok(line, " "), "solid"))
  {
    ROS_ERROR("Invalid mesh file! Make sure the file is given in ascii-format.");
    ros::shutdown();
  }
  assert(line = (char *) realloc(line, MaxLine));
  f.getline(line, MaxLine);
  int k = 0;
  visualization_msgs::Marker mf;
  while(0 != strcmp(strtok(line, " "), "endsolid") && !ros::isShuttingDown())
  {
    int q = 0;
    for(int i = 0; i<7; i++)
    {
      while(line[q] == ' ')
        q++;
      if(line[q] == 'v')
      {
        const double yawTrafo = 0.0;      // used to rotate the mesh before processing
        const double scaleFactor = 100.0;   // used to scale the mesh before processing
        const double offsetX = 0.0;       // used to offset the mesh before processing
        const double offsetY = 0.0;       // used to offset the mesh before processing
        const double offsetZ = 0.0;       // used to offset the mesh before processing
        geometry_msgs::Point vert;
        char* v = strtok(line+q," ");
        v = strtok(NULL," ");
        double xtmp = atof(v)/scaleFactor;
        v = strtok(NULL," ");
        double ytmp = atof(v)/scaleFactor;
        vert.x = cos(yawTrafo)*xtmp-sin(yawTrafo)*ytmp;
        vert.y =  sin(yawTrafo)*xtmp+cos(yawTrafo)*ytmp;
        v = strtok(NULL," ");
        vert.z =  atof(v)/scaleFactor;
        vert.x -= offsetX;
        vert.y -= offsetY;
        vert.z -= offsetZ;
        if(maxX<vert.x)
          maxX=vert.x;
        if(maxY<vert.y)
          maxY=vert.y;
        if(maxZ<vert.z)
          maxZ=vert.z;
        if(minX>vert.x)
          minX=vert.x;
        if(minY>vert.y)
          minY=vert.y;
        if(minZ>vert.z)
          minZ=vert.z;
        mf.points.push_back(vert);
      }
      assert(line = (char *) realloc(line, MaxLine));
      f.getline(line, MaxLine);
    }
    k++;
  }
  free(line);
  f.close();
  ROS_INFO("Mesh area is bounded by: [%2.2f,%2.2f]x[%2.2f,%2.2f]x[%2.2f,%2.2f]", minX,maxX,minY,maxY,minZ,maxZ);
  mf.header.frame_id = "/kopt_frame";
  mf.header.stamp = ros::Time::now();
  mf.type = visualization_msgs::Marker::TRIANGLE_LIST;
  mf.id = 0;
  mf.action = visualization_msgs::Marker::ADD;
  mf.color.r = 0.0;
  mf.color.g = 1.0;
  mf.color.b = 0.0;
  mf.color.a = 1.0;
  mesh->markers.push_back(mf);
  visualization_msgs::Marker me;
  int i = 0;
  geometry_msgs::Point p0;
  for (geometry_msgs::Point p : mf.points)
  {
    if (!me.points.empty())
        me.points.push_back(me.points.back());
    me.points.push_back(p);
    if (i % 3 ==0)
      p0 = p;
    else if (i % 3 == 2)
    {
      me.points.push_back(me.points.back());
      me.points.push_back(p0);
    }
    ++i;
  }
  me.header.frame_id = "/kopt_frame";
  me.header.stamp = ros::Time::now();
  me.type = visualization_msgs::Marker::LINE_LIST;
  me.id = 1;
  me.action = visualization_msgs::Marker::ADD;
  me.color.r = 0.0;
  me.color.g = 0.0;
  me.color.b = 1.0;
  me.color.a = 1.0;
  mesh->markers.push_back(me);
}
