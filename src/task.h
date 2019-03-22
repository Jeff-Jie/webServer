#ifndef _TASK_
#define _TASK_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

const char *path = "/home/jeff/桌面/ceshi/webServer-master";
const int BUFFER_SIZE = 4096;

class task
{
private:
	int connfd;

public:
	task(){}
	task(int fd):connfd(fd){}
	~task(){}

    //response函数就是将http应答报文发送给浏览器
    void response(char *message, int status) //message是响应正文，status是状态码
	{
        char buf[512]; //开辟一个512字节大小的char数组
		sprintf(buf, "HTTP/1.1 %d OK\r\nConnection: Close\r\n"
        "content-length:%ld\r\n\r\n", status, strlen(message)); //buf先存放状态行+响应头

        sprintf(buf, "%s%s", buf, message); //重复利用buf，省去再次开辟一个空间的花销
                                            //这次将状态行+响应头+响应正文放入buf中
        write(connfd, buf, strlen(buf)); //通过浏览器对应的connfd，将buf发送到浏览器
                                         //从而实现了将HTTP应答报文发送给浏览器
	}


    //将发送文件的HTTP应答报文发送给浏览器
    void response_file(int size, int status) //size是文件大小，status是状态码
	{
        char buf[128]; //开辟一个128字节大小的char数组
		    sprintf(buf, "HTTP/1.1 %d OK\r\nConnection: Close\r\n"
        "content-length:%d\r\n\r\n", status, size); //buf先存放状态行+响应头
        write(connfd, buf, strlen(buf)); //通过浏览器对应的connfd，将buf发送到浏览器
                                         //从而实现了将HTTP应答报文发送给浏览器

	}

	void response_get(char *filename);

	void response_post(char *filename, char *argv);

	void doit();
};


//入口函数
//一个浏览器连上了，就会有一个解析任务放到线程池任务队列
//线程池中的线程就会被唤醒，执行doit函数
//解析HTTP请求报文
void task::doit()
{
	char buffer[BUFFER_SIZE];
	int size;
    read:	size = read(connfd, buffer, BUFFER_SIZE - 1); //浏览器将输入的东西转换为HTTP请求报文发送到web服务器
                                                          //通过connfd发送过来的，所以从这里读取HTTP请求报文
	//printf("%s", buffer);
    if(size > 0) //读到数据
	{
        char method[5]; //存放请求行的请求方法
        char filename[50]; //存放浏览器要请求的资源，也就是浏览器要访问web服务器的什么文件（html/图片...）

        //请求行里的请求方法：
        int i, j;
		    i = j = 0;
        while(buffer[j] != ' ' && buffer[j] != '\0') //碰到空格 就代表请求方法字符 读取完毕
		    {
			    method[i++] = buffer[j++];
		    }
		    ++j;
        method[i] = '\0'; //请求方法读取完毕，最后加'\0',成为字符串

        //请求行里的请求资源（web服务器端的哪个文件）（含路径）：
		    i = 0;
        while(buffer[j] != ' ' && buffer[j] != '\0') //碰到空格 就代表请求资源字符 读取完毕
		    {
		    	filename[i++] = buffer[j++];
		    }
        filename[i] = '\0';//请求资源读取完毕，最后加'\0',成为字符串

        //根据不同的请求方法，进行不同操作，目前只支持GET和POST，所以用字符长度区分即可
        //如果请求方法是GET
        //strcasecmp，忽略大小，比较长度
        if(strcasecmp(method, "GET") == 0)  //GET method
		    {
			    response_get(filename);
		    }
        //如果请求方法是POST
        else if(strcasecmp(method, "POST") == 0)  //post method
		    {
			//printf("Begin\n");
            char argvs[100]; //存放请求正文
			memset(argvs, 0, sizeof(argvs));
			int k = 0;
			char *ch = NULL;
			++j;
			while((ch = strstr(argvs, "Content-Length")) == NULL)
			{
				k = 0;
				memset(argvs, 0, sizeof(argvs));
				while(buffer[j] != '\r' && buffer[j] != '\0')
				{
					argvs[k++] = buffer[j++];
				}
				++j;
				//printf("%s\n", argvs);
			}
			int length;
			char *str = strchr(argvs, ':');
			//printf("%s\n", str);
			++str;
			sscanf(str, "%d", &length);
			//printf("length:%d\n", length);
			j = strlen(buffer) - length;
			k = 0;
			memset(argvs, 0, sizeof(argvs));
			while(buffer[j] != '\r' && buffer[j] != '\0')
				argvs[k++] = buffer[j++];

			argvs[k] = '\0';
			//printf("%s\n", argvs);
			response_post(filename, argvs);
		}
        else //如果不是上述两种请求方法：
		{
            //那么就应该发送错误信息给浏览器
            char message[512]; //存放响应正文
            //sprintf把格式化的数据写入某个字符串，返回成功写入的字符串长度
            //把这些html语句写到message中
            sprintf(message, "<html><title>Tinyhttpd Error</title>");
            sprintf(message, "%s<body>\r\n", message);
            sprintf(message, "%s 501\r\n", message);
            sprintf(message, "%s <p>%s: Httpd does not implement this method", message, method);
            sprintf(message, "%s<hr><h3>The Tiny Web Server<h3></body>", message);
            response(message, 501); //调用response函数，将HTTP应答报文发送给浏览器
		}


		//response_error("404");
	}
    else if(size < 0) //如果没有读到数据
        goto read;  //那么就跳转到read处，进行读取数据

	sleep(3);  //wait for client close, avoid TIME_WAIT
	close(connfd);
}


void task::response_get(char *filename)
{
	char file[100];
    strcpy(file, path); //先将path路径复制到file数组

	int i = 0;
    bool is_dynamic = false; //是否是动态请求
	char argv[20];
	//查找是否有？号
	while(filename[i] != '?' && filename[i] != '\0')
		    ++i;
	if(filename[i] == '?')
	{	//有?号，则是动态请求
		int j = i;
		++i;
		int k = 0;
		while(filename[i] != '\0')  //分离参数和文件名
			argv[k++] = filename[i++];
		argv[k] = '\0';
		filename[j] = '\0';
		is_dynamic = true;
	}

    if(strcmp(filename, "/") == 0) //strcmp，比较两个字符串，若str1==str2，则返回零；若str1<str2，则返回负数
        strcat(file, "/index.html"); //extern char *strcat(char *dest, const char *src); 拼接
                                     //把src所指向的字符串（包括“\0”）复制到dest所指向的字符串后面（删除*dest原来末尾的“\0”）
    else //如果filename ！= "/"   应该肯定不等于"/"啊，奇怪了？
        strcat(file, filename); //拼接

    //现在file就是/home/jeff/桌面/ceshi/webServer-master/index.html这样的了

	//printf("%s\n", file);
	struct stat filestat;
    int ret = stat(file, &filestat);  //stat函数，通过文件名file获取文件信息，并保存在filestat所指的结构体stat中
                                      // 执行成功则返回0，失败返回-1，错误代码存于errno

    //如果获取文件信息失败，也就是没找到这个文件
    //那么就发送404错误到浏览器
	if(ret < 0 || S_ISDIR(filestat.st_mode)) //file doesn't exits
	{
        char message[512]; //存放HTTP应答报文的应答正文
		sprintf(message, "<html><title>Tinyhttpd Error</title>");
		sprintf(message, "%s<body>\r\n", message);
		sprintf(message, "%s 404\r\n", message);
		sprintf(message, "%s <p>GET: Can't find the file", message);
		sprintf(message, "%s<hr><h3>The Tiny Web Server<h3></body>", message);
		response(message, 404);
		return;
	}

    if(is_dynamic) //如果是动态请求
	{
        //那么就先将请求转交给web容器，再那里动态拼凑页面内容
		if(fork() == 0) 
		/*创建子进程执行对应的子程序，多线程中，创建子进程，
		只有当前线程会被复制，其他线程就“不见了”，这正符合我们的要求，
		而且fork后执行execl，程序被进程被重新加载*/
		{
			dup2(connfd, STDOUT_FILENO);  
			//将标准输出重定向到sockfd,将子程序输出内容写到客户端去。
            execl(file, argv); //执行子程序
		}
		wait(NULL);
	}
    else //如果是访问静态资源
	{
        int filefd = open(file, O_RDONLY); //以只读方式，打开这个文件，返回这个文件的描述符filefd，后面通过这个fd对该文件操作
        response_file(filestat.st_size, 200); //将该文件发送给浏览器
        //使用“零拷贝”sendfile发送文件
        //所谓零拷贝是指（与传统的read/write模式相比），在数据发送的过程中，不会产生用户态、内核态之间的数据拷贝
        //sendfile不能发送socket中的数据。
        sendfile(connfd, filefd, 0, filestat.st_size); //将filefd对应的文件，指定文件大小filestat.st_size，通过connfd发送到对应的浏览器
	}
}


void task::response_post(char *filename, char *argvs)
{
	char file[100];
	strcpy(file, path);

	strcat(file, filename);

	struct stat filestat;
	int ret = stat(file, &filestat);
	printf("%s\n", file);
	if(ret < 0 || S_ISDIR(filestat.st_mode)) //file doesn't exits
	{
		char message[512];
		sprintf(message, "<html><title>Tinyhttpd Error</title>");
		sprintf(message, "%s<body>\r\n", message);
		sprintf(message, "%s 404\r\n", message);
		sprintf(message, "%s <p>GET: Can't find the file", message);
		sprintf(message, "%s<hr><h3>The Tiny Web Server<h3></body>", 
			message);
		response(message, 404);
		return;
	}

	char argv[20];
	int a, b;
	ret = sscanf(argvs, "a=%d&b=%d", &a, &b);
	if(ret < 0 || ret != 2)
	{
		char message[512];
		sprintf(message, "<html><title>Tinyhttpd Error</title>");
		sprintf(message, "%s<body>\r\n", message);
		sprintf(message, "%s 404\r\n", message);
		sprintf(message, "%s <p>GET: Parameter error", message);
		sprintf(message, "%s<hr><h3>The Tiny Web Server<h3></body>", 
			message);
		response(message, 404);
		return;
	}
	sprintf(argv, "%d&%d", a, b);
	if(fork() == 0) 
	{
		dup2(connfd, STDOUT_FILENO);  
		//将标准输出重定向到sockfd,将子程序输出内容写到客户端去。
		execl(file, argv); //执行子程序
	}
	wait(NULL);
}



#endif
