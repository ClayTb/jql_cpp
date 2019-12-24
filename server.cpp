/*************************************************************************
    > File Name: Server.cpp
    > Author: MattJi
    > Created Time: 2019-12-18
	> brief: 用来检测客户端是否离线，使用curl和企业微新对接
	> g++ server.cpp -o server -lpthread 
 ************************************************************************/
#include<netinet/in.h>   // sockaddr_in
#include<sys/types.h>    // socket
#include<sys/socket.h>   // socket
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/select.h>   // select
#include<sys/ioctl.h>
#include<sys/time.h>
#include<iostream>
#include<vector>
#include<map>
#include<string>
#include<cstdlib>
#include<cstdio>
#include<cstring>
#include <thread>
//#include <curl/curl.h>
using namespace std;
#define BUFFER_SIZE 1024
#define URL "https"
 
#if 0
int offline(void)
{
    string data="";
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, URL);
        curl_easy_setopt(curl,)
         while(true)
        {
        data = ooflineQ.pop();
        char* data1 = curl_easy_easy_escape(curl, data.c_str(), 0);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data1));
        res = curl_eays_perform(curl);
        }

    }
       curl_easy_cleanup(curl);
    return 0;
} 
#endif 
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


struct RECORD
{
	int count;
	string hostname;
};
 
void* heart_handler(void* arg);
 
int log(string log)
{
char buffer[33]={};
    struct timeval tv;
    time_t curtime;
 FILE *fp;

           fp = fopen("/tmp/start.txt", "a");
           gettimeofday(&tv, NULL);
           curtime = tv.tv_sec;
           strftime(buffer, 30, "%Y-%m-%d-%T: ", localtime(&curtime));
           fputs(buffer, fp);
           fprintf(fp, "%s\n", log.c_str());
           fclose(fp);
    return 0;
}

class Server
{
private:
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;
    int listen_fd;    // 监听的fd
    int max_fd;       // 最大的fd
    fd_set master_set;   // 所有fd集合，包括监听fd和客户端fd   
    fd_set working_set;  // 工作集合
    struct timeval timeout;
    //map<int, pair<string, int> > mmap;   // 记录连接的客户端fd--><ip, count>
    //map<int, pair<string, pair<int, string>>> mmap; //fd--><ip, <count,hostname>>    
    map<int, pair<string, struct RECORD> > mmap; //fd--><ip, <count,hostname>>
    
public:
    Server(int port);
    ~Server();
    void Bind();
    void Listen(int queue_len = 20);
    void Accept();
    void Run();
    void Recv(int nums);
    friend void* heart_handler(void* arg);
};
 
Server::Server(int port)
{
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(port);
    // create socket to listen
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0)
    {
        cout << "Create Socket Failed!";
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}
 
Server::~Server()
{
    for(int fd=0; fd<=max_fd; ++fd)
    {
        if(FD_ISSET(fd, &master_set))
        {
            close(fd);
        }
    }
}
 
void Server::Bind()
{
    if(-1 == (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))))
    {
        cout << "Server Bind Failed!";
        exit(1);
    }
    cout << "Bind Successfully.\n"; 
}
 
void Server::Listen(int queue_len)
{
    if(-1 == listen(listen_fd, queue_len))
    {
        cout << "Server Listen Failed!";
        exit(1);
    }
    cout << "Listen Successfully.\n";
}
 
void Server::Accept()
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
 
    int new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if(new_fd < 0)
    {
        cout << "Server Accept Failed!";
        exit(1);
    }
 
    string ip(inet_ntoa(client_addr.sin_addr));    // 获取客户端IP
 
    cout << ip << " was accepted.\n";
    //log(ip+" new connection was accepted\n");
    struct RECORD rec={
		count:0, 
		hostname: ""};
    mmap.insert(make_pair(new_fd, make_pair(ip, rec)));
 
    // 将新建立的连接的fd加入master_set
    FD_SET(new_fd, &master_set);
    if(new_fd > max_fd)
    {
        max_fd = new_fd;
    }
}   
 
void Server::Recv(int nums)
{
    for(int fd=0; fd<=max_fd; ++fd)
    {
        if(FD_ISSET(fd, &working_set))
        {
            bool close_conn = false;  // 标记当前连接是否断开了
	    char recv_buffer[1028];
	    memset(recv_buffer, 0, sizeof recv_buffer);
 
		int _recv = -1;
           // _recv = recv(fd, &head, sizeof(head), 0);   // 先接受包头            
	     _recv = recv(fd, recv_buffer, sizeof(recv_buffer), 0);   // 先接受包头
	//	cout << recv_buffer <<endl;
        if(_recv < 1)
	    {
                close_conn = true;
                //cout << _recv << endl;
	    }
	    else
	    {
	        mmap[fd].second.count = 0;        // 每次收到心跳包，count置0
	        if(mmap[fd].second.hostname.empty())
		{
		    mmap[fd].second.hostname = recv_buffer;
		    
		    string host = mmap[fd].second.hostname+" online\n";
		    log(host);
		    string data1 = "curl 'https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1b3b3ed3-c3e3-464e-aaea-b7f0c75bf5da' -H 'Content-Type: application/json' -d '{\"msgtype\": \"text\",\"text\": {\"content\":\"" + host + "\"}}'";
            exec(data1.c_str());
		}
               send(fd, "rsp", 3, MSG_NOSIGNAL);   
               //send(fd, "rsp", 3, 0);   
	        //cout << "Received heart-beat from client.\n";
	    }
 
            if(close_conn)  // 当前这个连接有问题，关闭它
            {
                close(fd);
                FD_CLR(fd, &master_set);
                if(fd == max_fd)  // 需要更新max_fd;
                {
                    while(FD_ISSET(max_fd, &master_set) == false)
                        --max_fd;
                }
            }
        }
    }   
}
 
void Server::Run()
{
    pthread_t id;     // 创建心跳检测线程
    int ret = pthread_create(&id, NULL, heart_handler, (void*)this);
    if(ret != 0)
    {
        cout << "Can not create heart-beat checking thread.\n";
    }
 
    max_fd = listen_fd;   // 初始化max_fd
    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);  // 添加监听fd
 
    while(1)
    {
        FD_ZERO(&working_set);
        memcpy(&working_set, &master_set, sizeof(master_set));
 
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
 
        int nums = select(max_fd+1, &working_set, NULL, NULL, &timeout);
        if(nums < 0)
        {
            cout << "select() error!";
            exit(1);
        }
 
        if(nums == 0)
        {
            //cout << "select() is timeout!";
            continue;
        }
 
        if(FD_ISSET(listen_fd, &working_set))
            Accept();   // 有新的客户端请求
        else
            Recv(nums); // 接收客户端的消息
    }
}
 
 
// thread function
void* heart_handler(void* arg)
{
    cout << "The heartbeat checking thread started.\n";
    Server* s = (Server*)arg;
    while(1)
    {
        //map<int, pair<string, int> >::iterator it = s->mmap.begin();
        map<int, pair<string, struct RECORD> >::iterator it = s->mmap.begin();
        for( ; it!=s->mmap.end(); )
        {   
            if(it->second.second.count == 5)   // 3s*5没有收到心跳包，判定客户端掉线
            {
                cout << "The client " << it->second.second.hostname << " has be offline.\n";
                string data = it->second.second.hostname+" has be offline.";
		    string data1 = "curl 'https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1b3b3ed3-c3e3-464e-aaea-b7f0c75bf5da' -H 'Content-Type: application/json' -d '{\"msgtype\": \"text\",\"text\": {\"content\":\"" + data + "\"}}'";
            exec(data1.c_str());
                //offlineQ.push();
                log(it->second.second.hostname+" has be offline.\n");
                int fd = it->first;
                close(fd);            // 关闭该连接
                FD_CLR(fd, &s->master_set);
                if(fd == s->max_fd)      // 需要更新max_fd;
                {
                    while(FD_ISSET(s->max_fd, &s->master_set) == false)
                        s->max_fd--;
                }
 
                s->mmap.erase(it++);  // 从map中移除该记录
            }
            else if(it->second.second.count < 5 && it->second.second.count >= 0)
            {
                it->second.second.count += 1;
                ++it;
            }
            else
            {
                ++it;
            }
        }
        sleep(3);   // 定时三秒
    }
}
 
int main()
{
    Server server(15000);
    server.Bind();
    server.Listen();
    server.Run();
    //thread (offline).detach();
    
    while(1)
    {
        string msg;
	    cout << "getline";
        getline(cin, msg);
        if(msg == "exit")
            break;
        cout << "msg\n";
    }
    return 0;
}

