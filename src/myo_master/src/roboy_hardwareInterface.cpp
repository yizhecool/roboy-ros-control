#include "roboy_hardwareInterface.hpp"

Roboy::Roboy(){ 
    init_request_sub = nh.subscribe("/roboy/initRequest",1000, &Roboy::initializeCallback, this);
    init_response_pub = nh.advertise<common_utilities::InitializeResponse>("/roboy/initResponse",1);
    
    controller_manager_client = nh.serviceClient<controller_manager_msgs::LoadController>("/controller_manager/load_controller");
}

void Roboy::initializeCallback(common_utilities::InitializeRequest initrequest_msg){
    // allocate corresponding control arrays (4 motors can be connected to each ganglion)
    while(flexray.checkNumberOfConnectedGanglions()>6){
        ROS_WARN("Flexray interface says %d ganglions are connected, check cabels and power", flexray.checkNumberOfConnectedGanglions());
    }
    ROS_INFO("Flexray interface says %d ganglions are connected", flexray.checkNumberOfConnectedGanglions());
    
    cmd = new double[initrequest_msg.idList.size()];
    pos = new double[initrequest_msg.idList.size()];
    vel = new double[initrequest_msg.idList.size()];
    eff = new double[initrequest_msg.idList.size()];
    
    char motorname[10];

    common_utilities::InitializeResponse msg;
    common_utilities::ControllerState controllerState;
    
    for (uint i=0; i<initrequest_msg.idList.size(); i++){
        sprintf(motorname, "motor%d", initrequest_msg.idList[i]);
        // connect and register the joint state interface
        hardware_interface::JointStateHandle state_handle(motorname, &pos[i], &vel[i], &eff[i]);
        jnt_state_interface.registerHandle(state_handle);
        
        // connect and register the joint position interface
        hardware_interface::JointHandle pos_handle(jnt_state_interface.getHandle(motorname), &cmd[i]);
        jnt_pos_interface.registerHandle(pos_handle);

        controllerState.id = initrequest_msg.idList[i];
        controllerState.state = ControllerState::INITIALIZED;

        msg.controllers.push_back(controllerState);  //TODO: enum with status codes
    }
    
    registerInterface(&jnt_state_interface);
    registerInterface(&jnt_pos_interface);
    
    ROS_INFO("Hardware interface initialized");
    string str;
    vector<string> resources = jnt_pos_interface.getNames();
    for(uint i=0; i<resources.size();i++){
        str.append(resources[i]);
        str.append(" ");
    }
    ROS_INFO("Resources registered to this hardware interface:\n%s", str.c_str());
    ROS_INFO("Waiting for controller");

    init_response_pub.publish(msg);
    
    ready = true;
}

Roboy::~Roboy(){
    delete[] cmd;
    delete[] pos;
    delete[] vel;
    delete[] eff;
}

void Roboy::read(){
    ROS_DEBUG("read");
#ifdef HARDWARE
    flexray.exchangeData();
#endif
    uint i = 0;
    for (uint ganglion=0;ganglion<flexray.numberOfGanglionsConnected;ganglion++){ 
        // four motors can be connected to each ganglion
        for (uint motor=0;motor<4;motor++){ 
            pos[i] = flexray.GanglionData[ganglion].muscleState[motor].actuatorPos*flexray.controlparams.radPerEncoderCount;
            vel[i] = flexray.GanglionData[ganglion].muscleState[motor].actuatorVel*flexray.controlparams.radPerEncoderCount;
            eff[i] = flexray.GanglionData[ganglion].muscleState[motor].tendonDisplacement; // dummy TODO: use polynomial approx
            i++;
        }
    }
}
void Roboy::write(){
    ROS_DEBUG("write");
    uint i = 0;
    for (uint ganglion=0;ganglion<flexray.numberOfGanglionsConnected;ganglion++){ 
        // four motors can be connected to each ganglion
        for (uint motor=0;motor<4;motor++){ 
            if(ganglion<3)  // write to first commandframe
                flexray.commandframe0[ganglion].sp[motor] = cmd[i];
            else            // else write to second commandframe
                flexray.commandframe1[ganglion].sp[motor] = cmd[i];
            i++;
        }
    }
#ifdef HARDWARE
    flexray.updateCommandFrame();
    flexray.exchangeData();
#endif
}
