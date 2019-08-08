/*
Date: 2019-7-24
author: cti
brief: 提供wifi控梯的接口
    一共分为4部分：
    1. wifi切换部分
    2. 与梯控mqtt通信部分
    3. 接口函数部分
    4. 测试代码
    测试过程：
    1. rosrun tikong-wifi tikong-wifi
    2. please select:
		1. 呼梯(1 floor) 
		2. 开门(2 duration)
		3. 关门(3 duration)
		4. Exit
	3.发布正在进电梯消息
    rostopic pub -1 /robot_elevator_relationship cti_msgs/RobotNotifyState "id: '' robot_elevator_relationship: 1" 
    程序会自动连接wifi以及mqtt
    4. 发布已经在电梯内
    rostopic pub -1 /robot_elevator_relationship cti_msgs/RobotNotifyState "id: '' robot_elevator_relationship: 2"
    5. 呼梯1 2
    6. 到达目的楼层 发送开门10秒指令：2 10
    7. 发生关门：3 0
    8. 发布已经在电梯外
    rostopic pub -1 /robot_elevator_relationship cti_msgs/RobotNotifyState "id: '' robot_elevator_relationship: 0"
   

*/

#include <string.h>
#include "std_msgs/String.h"
#include "ros/ros.h"
#include "cti_msgs/RobotNotifyState.h"
#include <stdlib.h>
#include <thread>


//ros接收消息buffer大小
#define BUFSIZE 1000
//需要监听的机器人状态的topic
#define ROBOTTOPIC "robot_elevator_relationship"
//梯控wifi的ssid以及密码
#define SSID "tikong"
#define PWd "tikong-4g"
//工控机建立wifi的连接名称
#define NAME "tikong-wifi"
//工控机建立wifi的连接名称所在的位置
#define WPATH "/etc/NetworkManager/system-connections/"
//梯控的IP
#define TKIP "192.168.1.150"
//wifi端口名
#define WLAN "wlp2s0"
//梯控发布的状态topic
#define ELETOPIC "/cti/ele/state"
//发送给梯控的命令topic
#define CMDTOPIC "/cti/ele/cmd"

/*
{"ID":"", "requestID":"", "cmd":"call", "floorNum":""}
{"ID":"", "requestID":"", "cmd":"close", "duration":""}
{"ID":"", "requestID":"", "cmd":"open", "duration":""}
{"ID":"", "requestID":"", "cmd":"cancelclose"}
{"ID":"", "requestID":"", "cmd":"cancelopen"}
*/
/*
# robot_elevator_relationship 状态列表
uint8 ROBOT_ELEVATOR_OUTSIDE = 0  #机器人在电梯外
uint8 ROBOT_ELEVATOR_ENTERING = 1 #机器人正在进入电梯
uint8 ROBOT_ELEVATOR_INSIDE = 2   #机器人在电梯内
uint8 ROBOT_ELEVATOR_LEAVING = 3  #机器人正在离开电梯
uint8 robot_elevator_relationship
*/
using namespace std;
void mqtt_connect();
int mqtt_disconn();
void mqtt_setup();


//=========wifi 部分开始============
enum WifiState {
	disconn = 0, //未连接
	conn,  //已连接
}; 
enum RobotState {
	out = 0, //电梯外
	in,  //已经进入电梯
};
WifiState wstate = disconn;
RobotState rstate = out;


int connWifi()
{
    int ret = 1;
    //连接wifi
    ret = system("nmcli c up tikong-wifi");
    if(0 != ret)
    {
    	printf("tikong-wifi not up\n");
        return -1;
    }
    //测试wifi是否正常
    sleep(1);
    ret = system("ping 192.168.1.150 -c 1");
    if(0 == ret)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
int disConnWifi()
{
    int ret = 1;
    ret = system("nmcli c down tikong-wifi");
    //断开mqtt连接
    if(0 == ret)
    {
        ret = mqtt_disconn();
    }
    return ret;
}
void robotCallback(const cti_msgs::RobotNotifyState::ConstPtr& msg)
{
    int state = msg->robot_elevator_relationship;
    ROS_INFO("Robot: %d", state);
    int ret = -1;
    switch (state){
        // 正在进入电梯
        case cti_msgs::RobotNotifyState::ROBOT_ELEVATOR_ENTERING:
            ret = connWifi();
            if(0 == ret)
            {
            	printf("wifi connected\n");
                wstate = conn;
                //连接上wifi之后马上建立mqtt连接 
                mqtt_connect();
            }
            break;
        //机器人在电梯外
        case cti_msgs::RobotNotifyState::ROBOT_ELEVATOR_OUTSIDE:
            ROS_INFO("ROBOT_ELEVATOR_OUTSIDE");
            //需要检测上一个状态是不是在电梯里
            if(rstate == in)
            {
                ret = disConnWifi();
                if(0 == ret)
                {
                    wstate = disconn;   
                    printf("wifi disconnected\n");             
                }
            }
            rstate = out;
            break;
        //机器人在电梯里
        case cti_msgs::RobotNotifyState::ROBOT_ELEVATOR_INSIDE:
            ROS_INFO("ROBOT_ELEVATOR_INSIDE");
            rstate = in;
            break;           
        //机器人正在离开电梯 
        case cti_msgs::RobotNotifyState::ROBOT_ELEVATOR_LEAVING:
            ROS_INFO("ROBOT_ELEVATOR_LEAVING");
            break;
        default:
            break;
    }
}
                
//=========wifi部分结束=========

//========mqtt部分开始=========
#include <mosquitto.h>
struct mosquitto *mosq_wifi = NULL;
bool connected_mqtt = false;
string eleState = "";
bool changed = false;
//直接发送mqtt消息给梯控
int mqtt_send(struct mosquitto *mosq, string topic, const char *msg){
  return mosquitto_publish(mosq, NULL, topic.c_str(), strlen(msg), msg, 0, 0);
}
//接收到梯控数据的处理函数
void tkWifi(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
        //printf("topic: %s, payload: %s\n", message->topic, (char *)message->payload);
        string data = static_cast<char*>(message->payload);
        //将接收到的数据直接放到eleState
        if(eleState.compare(data) != 0)
        {
        	//printf("ele state changed\n");
        	eleState = data;
        	changed = true;
        }
        else
        {
        	//printf("ele state not changed\n");
        	//changed = false;
        }
        //printf(data.c_str());
        /*
        {
           "ID" : "00000",
           "door" : "closed",
           "floorNum" : "15",
           "floorNum_r" : "11",  //相对楼层
           "state" : "stop"
        }
        */
        
	}else{
		printf("message %s (null)\n", message->topic);
	}
}
//订阅成功后的callback
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
    printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
        printf(", %d", granted_qos[i]);
	}
	printf("\n");
}
//在成功连接之后就去订阅消息
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	int i;
	if(!result){
		/* Subscribe to broker information topics on successful connect. */
        //这里订阅梯控发布的消息
		mosquitto_subscribe(mosq, NULL, ELETOPIC, 2);
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}
void mqtt_setup()
{
    cout<<"set up tikong mqtt\n";
	bool clean_session = true;
  
    mosquitto_lib_init();
    mosq_wifi = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq_wifi){
		fprintf(stderr, "Error: Out of memory.\n");
		exit(1);
	}
  
      //与梯控通信回调函数
  	mosquitto_message_callback_set(mosq_wifi, tkWifi);
	mosquitto_subscribe_callback_set(mosq_wifi, my_subscribe_callback);
    mosquitto_connect_callback_set(mosq_wifi, my_connect_callback);
}
void mqtt_connect()
{   
    string host = TKIP;
	int port = 1883;
	int keepalive = 60;
    cout<<"connect tikong mqtt\n";
    if(mosquitto_connect(mosq_wifi, host.c_str(), port, keepalive)){
		fprintf(stderr, "Unable to connect.\n");
        printf("Unable to connect local.");
		exit(2);
	}
    connected_mqtt = true;
    int loop = mosquitto_loop_start(mosq_wifi);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        printf("Unable to start loop: %i\n", loop);
        exit(3);
    }
}

int mqtt_disconn()
{
    int ret = 1;
    ret = mosquitto_disconnect(mosq_wifi);
    if(0 == ret)
    {
        ret = mosquitto_loop_stop(mosq_wifi,true);
    }
    return ret;
}
//==========mqtt部分结束=========


//=========接口函数开始=========
void getStatus(const std::string& id) 
{
    //如果现在wifi已经连接上了
    if(conn == wstate)
    {
        //直接返回电梯状态
        if(true == changed)
        {
        	cout << eleState;
        	changed = false;
        }
    }
    else
    {
    	cout << "wifi not connected\n";
    }
}

//呼梯
int requestFloor(const std::string id, const std::string floorName, bool cancell)
{
    //拼接出命令
    string data = "{\"ID\":\"";
    data.append(id);
    data.append("\", \"requestID\":\"");
    //？
    data.append("abc-456-def");
    data.append("\", \"cmd\":\"call\", \"floorNum\":\"");
    data.append(floorName);
    data.append("\"}");
    cout << data <<endl;
    int ret = mqtt_send(mosq_wifi, "/cti/ele/cmd", data.c_str());
    if(ret != 0)
    {
        printf("mqtt_send error=%i\n", ret);
    }
}

//开门
void holdDoor(const std::string id, bool isOpen, int duration) 
{
    string data = "{\"ID\":\"";
    data.append(id);
    data.append("\", \"requestID\":\"");
    data.append("abc-123-def");
    //开门
    if(true == isOpen)
    {       
        data.append("\", \"cmd\":\"open\", \"duration\":\"");
        data.append(to_string(duration));
        data.append("\"}");
    }
    //关门
    else if(false == isOpen)
    {
        data.append("\", \"cmd\":\"cancelopen\"}");
    }
    cout << data <<endl;
    int ret = mqtt_send(mosq_wifi, "/cti/ele/cmd", data.c_str());
    if(ret != 0)
    {
        printf("mqtt_send error=%i\n", ret);
    }
}
//===========接口部分结束=========

//===========测试程序开始=========
#define FRE 1
//ros接收消息buffer大小
#define BUFSIZE 1000
#define ROBOTTOPIC "robot_elevator_relationship"
#define WIFINODE "wifi_control"

//手动输入命令
int inputThread(void)
{
	int i;
    int j;
    while(true)
    {
        printf("please select:\n1. 呼梯(1 floor) \n2. 开门(2 duration)\n3. 关门(3 duration)\n4. Exit\n");
        scanf("%d %i", &i, &j);
        //printf("input %d, %d", i, j);
        switch(i)
        {
            case 3:
                printf("关门\n");
                holdDoor("00005", false, j); 
                //holdDoor("00005", true, to_string(j)); 
                break;
            case 1:
                printf("呼梯");
                requestFloor("00005", to_string(j), true);
                break;
            case 2:
                printf("开门");
                holdDoor("00005", true, j); 
                break;
            case 4:
                printf("Exit\n");
                exit(0);
                break;
            default:        
                printf("error input %d\n", i);
                break;
        }   
    }

}

//初始化tikong wifi接口以及mqtt ros，程序启动是需要调用
int main(int argc, char* argv[])
{
    thread (inputThread).detach();

        //查看wifi是否之前连接过
    int ret = system("ls /etc/NetworkManager/system-connections/tikong-wifi");
    //如果没有，需要建立wifi连接文件
    if(0 != ret)
    {
    	string cmd = "nmcli c add type wifi con-name tikong-wifi ifname ";
    	cmd.append(WLAN);
    	cmd.append(" ssid tikong");
        //system("nmcli c add type wifi con-name tikong-wifi ifname wlp3s0 ssid tikong");
        system(cmd.c_str());
        //autoconnection设置成false，不自动连接
        system("nmcli c modify tikong-wifi conn.autoconnect false wifi-sec.key-mgmt wpa-psk wifi-sec.psk tikong-4g");
    }
    //只初始化mqtt，现在wifi还没有连接，不连接梯控mqtt
    mqtt_setup();
    //初始化ros
    ros::init(argc, argv, WIFINODE);
    ros::NodeHandle m, q;
    ros::Subscriber sub = q.subscribe(ROBOTTOPIC, BUFSIZE, robotCallback);
    
    ros::Rate loop_rate(FRE);
    while (ros::ok())
    {
        //发布电梯状态ros topic
        getStatus("00005");
        ros::spinOnce();
		loop_rate.sleep();
    }

    return 0;
}
//===========测试程序结束=========
