/*************************************************************************
    > File Name: Client.cpp
    > Author: MattJi
    > Created Time: 2019-12-18
	> nohup ./client >/dev/null 2>&1 &
	> 已经加入到电梯控制的startup.bash里
	> 加入电梯检测程序
 ************************************************************************/
#include<netinet/in.h>   // sockaddr_in
#include<sys/types.h>    // socket
#include<sys/socket.h>   // socket
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<iostream>
#include<string>
#include<cstdlib>
#include<cstdio>
#include<cstring>
using namespace std;
#define BUFFER_SIZE 1024
 
enum Type {HEART, OTHER};
 
struct PACKET_HEAD
{
    Type type;
    string hostname;
    int length;
};
 
bool reconn = true;
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>

std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}


void* send_heart(void* arg); 
 
class Client 
{
private:
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;
    int fd;
public:
    Client(string ip, int port);
    ~Client();
    void Connect();
    void Run();
    friend void* send_heart(void* arg); 
};
 
Client::Client(string ip, int port)
{
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if(inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) == 0)
    {
        cout << "Server IP Address Error!";
        exit(1);
    }
    server_addr.sin_port = htons(port);
    server_addr_len = sizeof(server_addr);
    // create socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        cout << "Create Socket Failed!";
        exit(1);
    }
}
 
Client::~Client()
{
    close(fd);
}
 
void Client::Connect()
{
    cout << "Connecting......" << endl;
    if(connect(fd, (struct sockaddr*)&server_addr, server_addr_len) < 0)
    {
        cout << "Can not Connect to Server IP!";
        close(fd);
        //exit(1);
    }
    else
    {
        cout << "Connect to Server successfully." << endl;
        reconn = false;
    }
}
 
void Client::Run()
{
    pthread_t id;
    int ret = pthread_create(&id, NULL, send_heart, (void*)this);
    if(ret != 0)
    {
        cout << "Can not create thread!";
        exit(1);
    }
}
 
// thread function
void* send_heart(void* arg)
{
    cout << "The heartbeat sending thread started.\n";
    Client* c = (Client*)arg;
    int count = 0;  // 测试
	string data = exec("hostname");
    int ret = -1;
    char recv_buf[10];
    memset(recv_buf, 0, sizeof recv_buf);
	struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    while(1) 
    {   
        send(c->fd, data.c_str(), strlen(data.c_str())-1, MSG_NOSIGNAL);
		ret = recv(c->fd, recv_buf, sizeof(recv_buf), 0);
        //sleep(3);     // 定时3秒
        if(ret < 1)
		{
			//perror("recv ");
			reconn = true;
			c->~Client();
			break;
		}
		else
		{
			//cout << recv_buf << endl;
		}
        sleep(3);     // 定时5秒
        //++count;      // 测试：发送15次心跳包就停止发送
        //if(count > 15)
        //    break;
        
    }
}
 
int main()
{
#if 0
    while(1)
    {
        if(reconn)
        {
            cout << "1"<< endl;
            Client client("120.78.152.73", 15000);
            //Client client("127.0.0.1", 15000);
            client.Connect();
            //if(!reconn)
                client.Run();
        }
        sleep(3);
    }
 #endif   
    #if 1
Start:
    Client client("120.78.152.73", 15000);
    //Client client("127.0.0.1", 15000);
    client.Connect();
    if(!reconn)
        client.Run();
                
    while(1)
    {
        sleep(3);
        if(reconn)
            goto Start;
    }
    #endif
    return 0;
}

