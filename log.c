#include <stdlib.h> //getenv malloc free
#include <stdio.h> //fwrite
#include <time.h>
// for getpid
#include <unistd.h>
// for va_start etc functions
#include <stdarg.h>
#include <string.h>

#include <time.h> //// for strftime
#include <sys/time.h>

#include <sys/stat.h> //mkdir
#include <sys/types.h> //mkdir
#include <fcntl.h> //O_RDONLY
#include <errno.h> //errno and EEXIST
#include <string>
  
//unsigned char LOGFILE[50];
  
//_Bool LogCreated = 0;   
#define Log(level, format...) log(__FILE__, __LINE__, level, format)

std::string path;
int initlog()
//int main()
{
    char *home = NULL;
    if((home = getenv("HOME")) == NULL)
	{
		printf("no match\n");
		return 1;
	}
    //int l_home = strlen(home);
    char buffer[33];
  	struct timeval tv;
  	time_t curtime;
  	gettimeofday(&tv, NULL); 
  	curtime=tv.tv_sec;
  	strftime(buffer,30,"/%Y-%m-%d-%T.log",localtime(&curtime));
    int l_time = strlen(buffer);
    
   // std::string path;
    path.append(home);
    path.append("/log");

    //const char *path_c = path.c_str();
    if(mkdir(path.c_str(), 0770) == -1)
	{
		perror("create log folder");
		if(errno != EEXIST)
			return 1;
	}
    
    path.append(buffer);
    FILE *file;
    file = fopen(path.c_str(), "a");
    if(file == NULL)
    {
        perror("create file");
        return 2;
    }
    return 0;
}

  
void log(const char *filename, int line, int level, const char *format, ...)  
{  
    FILE *file;   
    //char buf[28];  
    time_t ts;  
    va_list vlist;  
  
    //time(&ts);  
    char buffer[30];
    struct timeval tv;
    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;
    strftime(buffer,30,"%Y-%m-%d-%T",localtime(&curtime));

//这里log文件肯定已经建立，直接打开    
    file = fopen(path.c_str(), "a");   
    if (file == NULL)   
    {   
        perror("open file");
        return;   
    }   
    else   
    {   
        fprintf(file, "[%s.%6ld]", buffer, tv.tv_usec);
        va_start(vlist, format);  
        vfprintf(file, format, vlist);  
        va_end(vlist);  
        fputc('\n', file);
        fclose(file);   
    }     
}

int main(void)
{
    int ret = initlog();
    if(ret != 0)
    {
        printf("init log file error, exit");
        return 1;
    }
    
    Log(0, "ERROR: sigaction(): %s", strerror(errno));
    Log(0, "test again");
    
}