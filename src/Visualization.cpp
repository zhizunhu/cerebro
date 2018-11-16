#include "Visualization.h"

Visualization::Visualization( ros::NodeHandle &nh )
{
    b_run_thread = false;
    this->nh = nh;

}

void Visualization::setDataManager( DataManager* dataManager )
{
    this->dataManager = dataManager;
    m_dataManager_available = true;
}

void Visualization::setCerebro( Cerebro* cerebro )
{
    this->cerebro = cerebro;
    m_cerebro_available = true;
}


void Visualization::setVizPublishers( const string base_topic_name )
{
    //
    // Test Publisher
    string pub_topic_test = base_topic_name+"/chatter";
    ROS_INFO( "Visualization Publisher pub_topic_test: %s", pub_topic_test.c_str() );
    chatter_pub = nh.advertise<std_msgs::String>(pub_topic_test, 1000);

    //
    // Publish Markers of frame data
    string framedata_pub_topic = base_topic_name+"/framedata";
    ROS_INFO( "Visualization Publisher framedata_pub_topic: %s", framedata_pub_topic.c_str() );
    framedata_pub = nh.advertise<visualization_msgs::Marker>(framedata_pub_topic, 1000);

}

void Visualization::run( const int looprate )
{
    assert( m_dataManager_available && "You need to set the DataManager in class Visualization before execution of the run() thread can begin. You can set the dataManager by call to Visualization::setDataManager()\n");
    assert( b_run_thread && "you need to call run_thread_enable() before run() can start executing\n" );
    assert( looprate > 0  && "[ Visualization::run] looprate need to be postive\n");

    ros::Rate rate( looprate );
    while( b_run_thread )
    {
        // cout << "Visualization::run() " << dataManager->getDataMapRef().size() <<endl;
        this->publish_frames();
        this->publish_loopcandidates();
        this->publish_test_string();

        rate.sleep();
    }
}


// #define __Visualization__publish_loopcandidates(msg) msg
#define __Visualization__publish_loopcandidates(msg) ;
void Visualization::publish_loopcandidates()
{
    assert( m_dataManager_available && m_cerebro_available && "You need to set cerebro and DataManager in class Visualization  before execution of the run() thread can begin. You can set the cerebro by call to Visualization::setCerebro() and dataManager as setDataManager.\n");

    // last 10 publish
    int n = cerebro->foundLoops_count();

    int start = max( 0, n - 10 );
    // 5% of the time start from 0.
    if( rand() % 100 < 2 ) start = 0;
    __Visualization__publish_loopcandidates(cout << "[Visualization::publish_loopcandidates] start=" << start << " end=" << n << endl;)

    if( n <= 0 ) return;

    auto data_map = dataManager->getDataMapRef();
    visualization_msgs::Marker marker;
    RosMarkerUtils::init_line_marker( marker );
    marker.ns = "loopcandidates_line";
    RosMarkerUtils::setcolor_to_marker( 1.0, 0.0, 0.0 , marker );
    // TODO If need be can also publish text for each (which could be score of the edge)


    for( int i=0 ; i<n ; i++ ) {

        marker.id = i;

        auto u = cerebro->foundLoops_i( i );
        ros::Time t_curr = std::get<0>(u);
        ros::Time t_prev = std::get<1>(u);
        double score = std::get<2>(u);

        assert( data_map.count( t_curr ) > 0 && data_map.count( t_prev ) > 0  && "One or both of the timestamps in foundloops where not in the data_map. This cannot be happening...fatal...\n" );
        int idx_1 = std::distance( data_map.begin(), data_map.find( t_curr )  );
        int idx_2 = std::distance( data_map.begin(), data_map.find( t_prev )  );

        Vector4d w_t_curr = data_map[t_curr]->getPose().col(3);
        Vector4d w_t_prev = data_map[t_prev]->getPose().col(3);

        // TODO - need to test. looks like alright.
        // add_point_to_marker with w_t_curr
        // add_point_to_marker with w_t_prev
        RosMarkerUtils::add_point_to_marker( w_t_curr, marker, true );
        RosMarkerUtils::add_point_to_marker( w_t_prev, marker, false );



        framedata_pub.publish( marker );
    }

}

// #define __Visualization__publish_frames( cmd ) cmd
#define __Visualization__publish_frames( cmd ) ;
void Visualization::publish_frames()
{
    assert( m_dataManager_available && "You need to set the DataManager in class Visualization before execution of the run() thread can begin. You can set the dataManager by call to Visualization::setDataManager()\n");

    // Adjust these manually to change behaviour
    bool publish_camera_visual = false;
    bool publish_camera_as_point = true;
    bool publish_txt = true;
    bool publish_verbose_txt = false;


    static std::map< ros::Time, int > XC;

    __Visualization__publish_frames(cout << TermColor::RED() << "---" << TermColor::RESET() << endl;)
    __Visualization__publish_frames(cout << "start... sizeof(XC)=" << XC.size() << endl;)
    auto data_map = dataManager->getDataMapRef();
    auto pose0 = dataManager->getPose0Stamp();


    visualization_msgs::Marker cam_vis; //visualize every pose as a cam-visual
    RosMarkerUtils::init_camera_marker( cam_vis , .5 );
    cam_vis.ns = "cam_pose_vis";


    visualization_msgs::Marker pt_vis; //visualize every pose as a point.
    RosMarkerUtils::init_points_marker( pt_vis );
    geometry_msgs::Point zer; zer.x =0.; zer.y=0.; zer.z = 0;
    pt_vis.points.push_back( zer );
    pt_vis.ns = "cam_pose_pt";
    pt_vis.scale.x = 0.015;
    pt_vis.scale.y = 0.015;


    visualization_msgs::Marker txt_vis;
    RosMarkerUtils::init_text_marker( txt_vis );
    txt_vis.ns = "cam_pose_txt";
    txt_vis.scale.z = 0.03;


    for( auto it = data_map.begin() ; it != data_map.end() ; it++ )
    {
        if( XC[ it->first ] < 10 || rand() % 100 < 2 ) { // publish only if not already published 10 times
            int seq_id = std::distance( data_map.begin() , it );
            cam_vis.id = seq_id;
            pt_vis.id = seq_id;
            txt_vis.id = seq_id;

            // Set Colors
            if( it->second->isKeyFrame() ) {
                RosMarkerUtils::setcolor_to_marker( 0., 1., 0. , cam_vis );
                RosMarkerUtils::setcolor_to_marker( 0., 1., 0. , pt_vis );
                RosMarkerUtils::setcolor_to_marker( 1., 1., 1. , txt_vis );

                // if( it->second->isWholeImageDescriptorAvailable() )
                    // RosMarkerUtils::setcolor_to_marker( 0., 0, 1. , cam_vis );
            }
            else {
                RosMarkerUtils::setcolor_to_marker( 1., 0., 0. , cam_vis );
                RosMarkerUtils::setcolor_to_marker( 1., 0., 0. , pt_vis );
                RosMarkerUtils::setcolor_to_marker( 1., 1., 1. , txt_vis );
            }


            // Set Pose
            if( it->second->isPoseAvailable() ) {
                auto wTc = it->second->getPose();
                RosMarkerUtils::setpose_to_marker( wTc , cam_vis );
                RosMarkerUtils::setpose_to_marker( wTc , pt_vis );

                RosMarkerUtils::setpose_to_marker( wTc , txt_vis );
                txt_vis.text = "";
                txt_vis.text += std::to_string(seq_id) + ";";
                if( publish_verbose_txt )  {
                    txt_vis.text += std::to_string( (it->first - pose0).toSec() ) +";";
                    txt_vis.text += std::to_string( (it->first).toSec() ) +";";
                }

                if( publish_camera_visual )
                    framedata_pub.publish( cam_vis );
                if( publish_camera_as_point )
                    framedata_pub.publish( pt_vis );
                if( publish_txt || publish_verbose_txt )
                    framedata_pub.publish( txt_vis );
            }


            if( it->second->isKeyFrame() )
                __Visualization__publish_frames(cout << TermColor::GREEN() );
            __Visualization__publish_frames( cout << "Publish seq_id=" << seq_id << "\t xc=" << XC[ it->first ] << "\t t="<< it->first - pose0 << "\t" << it->first << TermColor::RESET() << endl; )

            XC[ it->first ]++;
        }
    }
    __Visualization__publish_frames( cout << "Done... sizeof(XC)=" << XC.size() << endl; )
}
/*
void Visualization::publish_frames()
{
    assert( m_dataManager_available && "You need to set the DataManager in class Visualization before execution of the run() thread can begin. You can set the dataManager by call to Visualization::setDataManager()\n");

    static std::map<ros::Time, bool > XC;
    static visualization_msgs::Marker linestrip_marker;
    RosMarkerUtils::init_line_strip_marker( linestrip_marker );
    linestrip_marker.ns = "cam_line_strip";
    linestrip_marker.id = 0;


    auto data_map = dataManager->getDataMapRef();
    cout << "---\n";
    if( data_map.begin() == data_map.end() ) {
        cout << "nothing to vizualize\n";
        return;
    }

    // cout << data_map.rbegin()->first << "\t"<<  data_map.rbegin()->first - dataManager->getPose0Stamp() << endl;
    ros::Time lb = data_map.rbegin()->first - ros::Duration(3);
    for( auto it = data_map.lower_bound( lb ); it != data_map.end() ; it++ ) {
        // cout << std::distance( it, data_map.begin() ) << "] " << it->first << "\t" << it->first - dataManager->getPose0Stamp() << endl;

        if( XC.count( it->first) == 0 ) {
            cout << "sizeof(XC)=" << XC.size() << "  "<< it->first << "\t" << it->first - dataManager->getPose0Stamp() << endl;

            if( it->second->isPoseAvailable() ) {
                auto w_T_c = it->second->getPose();
                geometry_msgs::Point pt;
                pt.x = w_T_c(0,3);
                pt.y = w_T_c(1,3);
                pt.z = w_T_c(2,3);
                linestrip_marker.points.push_back( pt );
                framedata_pub.publish( linestrip_marker );
                XC[it->first] = true;
            }
        }
    }
    return ;

}
*/

void Visualization::publish_test_string()
{
    std_msgs::String msg;
    std::stringstream ss;
    ss << "hello world " << ros::Time::now();
    msg.data = ss.str();
    chatter_pub.publish( msg );
}
