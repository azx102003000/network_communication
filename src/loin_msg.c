#include <stdio.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "kernel_list.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cJSON.h"
#include <netdb.h>
#include <sys/ioctl.h>

#include <net/if.h>


int socketfd=0;//让所有线程共享
int port_h=6722;//共享端口
int down_date=0;//如果在传输的时候不允许crtl+c退出的标志位
struct  list_head  *head=NULL;

//用户自定义的内核链表结构体
struct  kernel_list
{
char  ip[64];  //数据域
int   port;
int   sockey;

struct list_head  list; //地址域
};

//加入新节点到内核链表中
int kerlist_add(struct  list_head *head,int sockey,int port,char *ip)
{
	
	struct  kernel_list  *new =  (struct  kernel_list*)malloc(sizeof(struct  kernel_list));
						  if(new ==NULL)
						  {
							  printf("creat node fail\n");
							  return -1;
						  }
					new->sockey = sockey;	  
					new->port   = port; //赋值端口号  
					strcpy(new->ip,ip);
					
					//插入新节点
				    list_add_tail(&new->list,head);
	
	
					return  1;
}

//显示好友列表
void show_list(struct  list_head *head)
{
	
	//3.取出节点的数据 
	struct list_head  *pos=NULL;
	int i=0;
    //遍历链表
	list_for_each(pos,head)
	{						  	//得到大结构体的数据
		struct  kernel_list  *p = list_entry(pos,struct  kernel_list,list);
	    printf("用户号:%d,IP:%s,端口:%d\n",i++/*,p->sockey*/,p->ip,p->port);
	
	}
	
}

//检查链表中是否有相同的IP地址
int ck_list(struct  list_head *head,char *ip)
{
	
	struct list_head  *pos=NULL;
	int i=0;
    //遍历链表
	list_for_each(pos,head)
	{						  	//得到大结构体的数据
	struct  kernel_list  *p = list_entry(pos,struct  kernel_list,list);
		
	    //printf("当前用户号:%d,IP:%s,port:%d\n",i++,p->ip,p->port);
		if(strcmp(p->ip,ip) == 0)
		{
			return 1;  //检查到相同则返回0 
		}
	}
	
	return 0;
}

//删除节点函数   修改删除接口 让他可以删除多个数据
void del_node(struct list_head  *head,char  *ip)
{
	
	//遍历链表找到需要删除的数据
	struct list_head  *pos  = head->next;
	struct list_head  *tmp  = NULL;
	list_for_each_safe(pos,tmp,head)  //安全的遍历
	{
		//取数据  tmp 保存大结构体的首地址
		struct kernel_list  *tmp  =  list_entry(pos,struct kernel_list,list);
			
				if(strcmp(tmp->ip,ip)==0)
				{
					//掉删除函数
					list_del(pos);
					
					 //释放堆空间 
					 free(tmp);
				}
	}	
		
}

void *func(void *arg)//UDP接收广播回播，UDP字符接收
{
	//创建头节点
	head =  (struct  list_head*)malloc(sizeof(struct  list_head));
	
	//1.初始化头节点 
	INIT_LIST_HEAD(head);

	char buf[1024]={0};
	struct sockaddr_in  seraddr1={0};  //保持发送者的IP地址信息
	int len=sizeof(seraddr1);//直接使用结构体的大小对len进行初始 
		while(1)
		{
																
			bzero(buf,1024);
			recvfrom(socketfd,buf,1024,0,(struct sockaddr*)&seraddr1,&len);				
			if(strcmp(buf,"join")!=0&&strcmp(buf,"exit")!=0)//不显示加入和退出
			{
				printf("发送者的信息为:IP:%s,端口=%d\n",inet_ntoa(seraddr1.sin_addr),ntohs(seraddr1.sin_port));
				printf("bufUDP = %s",buf);
				
			}
			
			if(!strcmp(buf,"join")) //收登录信息
			{
				//检查是否有相同的IP地址假设有则跳过添加的步骤进入下一次循环 
				if(ck_list(head,inet_ntoa(seraddr1.sin_addr)))
				{
					
					continue; //进入到下次循环
				}
				
				//加入到好友列表中
				kerlist_add(head,socketfd,ntohs(seraddr1.sin_port),inet_ntoa(seraddr1.sin_addr));
				
				//显示当前的好友列表
				printf("目前好友列表如下\n");
				show_list(head);
				
				//数据的回发  
				sendto(socketfd,"join", strlen("join"), 0, (struct sockaddr *)&seraddr1,sizeof(seraddr1)); 
				
				
			}
			
			if(!strcmp(buf,"exit")) //收退出信息
			{
				printf("用户退出IP:%s,端口=%d\n",inet_ntoa(seraddr1.sin_addr),ntohs(seraddr1.sin_port));
				// printf("发送者的信息为:IP:%s,端口=%d\n",inet_ntoa(seraddr1.sin_addr),ntohs(seraddr1.sin_port));
				del_node(head,inet_ntoa(seraddr1.sin_addr));
			}

		} 
	
		
}

void *func1(void *arg)//发送信息的集合
{

	int a=0;;
	while(1)
	{
		a=0;
		printf("1.显示好友列表 2.文字聊天 3.文件传输\n");
		scanf("%d",&a);
		while(getchar()!='\n');

		switch(a)
		{
			case 1:
			{
				printf("显示好友列表中\n");
				show_list(head);
				break;
			}
			case 2:   //文字聊天   UDP
			{
				char ip_name[20]={0};
				char mo[10]={0};
				printf("文字聊天\n");
				printf("输入对方IP号\n");
				scanf("%s",ip_name);
				
				if(!ck_list(head,ip_name)) //判断输入的IP在链表里是否拥有
				{
					printf("当前没有这个IP：%s好友,请在好友列表里查看\n",ip_name);
					continue;
				}
				printf("1.UDP  2.TCP  回车默认UDP\n");
				read(0,mo,10);
				if(!strcmp(mo,"\n"))
				{
					bzero(mo,10);
					strcpy(mo,"1\n");
				}
				
				if(!strcmp(mo,"2\n"))  //TCP 文字聊天
				{
					printf("TCP\n");
					char lai[1024]={0};
					int read_num=0;
					int clien = socket(AF_INET,SOCK_STREAM,0);
					if(clien<0)
					{
						printf("TCP客户端创建失败\n");
						break;
					}
					struct sockaddr_in tx_file={0};
					tx_file.sin_family      =AF_INET;
					tx_file.sin_port		=htons(port_h);
					tx_file.sin_addr.s_addr =inet_addr(ip_name);
					
					int ret =connect(clien,(struct sockaddr *)&tx_file,sizeof(tx_file));
					if(ret<0)
					{
						printf("链接服务器失败\n");
						break;
					}
					strcpy(lai,"tcpchat");
					write(clien,lai,strlen(lai));
					while(1)
					{
						bzero(lai,1024);//清空
						printf("输入发送的内容,输入exit返回\n");
						read_num=read(0,lai,1024);
						if(!strcmp(lai,"exit\n"))
						{
							break;
						}
						write(clien,lai,read_num);
						
					}
					
					close(clien);	
				}
				
				
				
				if(!strcmp(mo,"1\n")) //UDP 文字聊天
				{
					
				
					struct sockaddr_in  send={
					.sin_family			=AF_INET,
					.sin_port			=htons(port_h),
					.sin_addr.s_addr	=inet_addr(ip_name)
					};
					
						while(1)
						{
							char buf[1024]={0};
							printf("输入发送的内容,输入exit返回\n");
							read(0,buf,1024);
							if(!strcmp(buf,"\n"))
							{
								continue;
							}
							if(!strcmp(buf,"exit\n"))
							{
								break;
							}
							sendto(socketfd,buf,strlen(buf),0,(struct sockaddr *)&send,sizeof(send));
							
						}
						
				}

				break;
			}
			case 3:  //处理发送文件    TCP
			{
				
				char ip_name[20]={0};
				char tcp_exchange[20]={"tcp_exchange"};
				printf("文件传输\n");
				printf("输入IP地址\n");
				scanf("%s",ip_name);
				
				if(!ck_list(head,ip_name))
				{
					printf("当前没有这个IP：%s好友,请在好友列表里查看\n",ip_name);
					continue;
				}
				
				int clien = socket(AF_INET,SOCK_STREAM,0);  //创建TCP
				if(clien<0)
				{
					printf("TCP客户端创建失败\n");
					break;
				}
				
				struct sockaddr_in tx_file={0};
				tx_file.sin_family      =AF_INET;
				tx_file.sin_port		=htons(port_h);
				tx_file.sin_addr.s_addr =inet_addr(ip_name);
				
				int ret =connect(clien,(struct sockaddr *)&tx_file,sizeof(tx_file));//链接服务器
				if(ret<0)
				{
					printf("链接服务器失败\n");
					break;
				}
				
				
				int add_file=0,read_file=0,write_file=0,max_file=0;
				int fd=0,i=0;
				char buf[1024]={0};
				char buf2[1024]={0};
				char buf3[4096]={0};
				struct stat sb;
				write(clien,tcp_exchange,strlen(tcp_exchange));
				while(1)
				{
					add_file=0,read_file=0,write_file=0,max_file=0;
					bzero(buf,1024);bzero(buf2,1024);
					printf("输入发送的文件 输入exit退出\n");
					scanf("%s",buf);
					if(!strcmp(buf,"exit"))//输入exit返回
					{
						break;
					}
					if(!stat(buf,&sb))//判断发送的是否是目录，如果是就跳过
					{
						if(sb.st_mode&S_IFDIR)
						{
							printf("输入发送的文件是目录，请重新传输\n");
							continue;
						}
						
					}
					
					
					fd = open(buf,O_RDONLY);//发送者发送的文件在目录下没有就会提示
					if(fd<0)
					{
						printf("打开文件%s失败,请重新发送\n",buf);
						continue;
					}
					max_file = lseek(fd,0,SEEK_END);//计算大小
					lseek(fd,0,SEEK_SET);//回到文件开头
					bzero(buf2,1024);
					sprintf(buf2,"%s %d",buf,max_file);
					write(clien,buf2,strlen(buf2));
					
					while(1)
					{
						bzero(buf3,4096);
						down_date=1;
						read_file = read(fd,buf3,4096);
						add_file+=read_file;
						write(clien,buf3,read_file);
						
						if(add_file==max_file)
						{
							printf("传输完成\n");
							down_date=0;
							close(fd);
							break;
						}
							
					}
					
					
				}
				close(clien);
				break;
			}
			default :
			{
				printf("输入无效 重新输入\n");
				break;
			}
			
		}
		
		
	}
	
}
//处理  TCP接收文件和接收文字  
void *func2(void *arg)
{
	char buf[4096]={0};
	char buf2[256]={0};
	char buf3[256]={0};
	char buf4[256]={0};
	int fd=0,ret=0;
	int size=0,i=0;
	int read_file=0,all_file=0;
	int read_num=0;
	int newsocket = *(int*)arg;
	while(1)
	{
		//清空
		fd=0,ret=0,size=0,i=0;
		bzero(buf,4096),bzero(buf2,256);
		bzero(buf3,256),bzero(buf4,256);
		read_file=0,all_file=0;
		
		ret=read(newsocket,buf,sizeof(buf)); //阻塞
		if(ret > 0)
		{
			
			if(!strcmp(buf,"tcpchat"))  //文字聊天 
			{
				while(1)
				{
					bzero(buf,4096);
					read_num=read(newsocket,buf,4096);
					if(read_num==0)   //读到为0就退出
					{
						down_date=0;
						close(newsocket);
						pthread_exit(NULL);
						// break;
					}
					if(strcmp(buf,"\n"))//发送回车不打印
					{
						printf("bufTCP=%s",buf);  //read会带上\n就不用了
					}
					
				}
				

				
			}
			else if(strcmp(buf,"tcpchat")!=0&&strcmp(buf,"tcp_exchange")!=0)//其他客户端的TCP链接进行聊天打印
			{
				printf("other_TCP=%s\n",buf);
				while(1)
				{
					bzero(buf,4096);
					read_num=read(newsocket,buf,4096);
					if(read_num==0)   //读到为0就退出
					{
						down_date=0;
						close(newsocket);
						pthread_exit(NULL);
						// break;
					}
					printf("other_TCP=%s\n",buf);
					
				}
				
			}
			
			else if(!strcmp(buf,"tcp_exchange")) //文件接收
			{
				bzero(&ret,sizeof(ret));
				bzero(buf,4096);
				ret=read(newsocket,buf,4096);
				sscanf(buf,"%s %d",buf2,&size); 
				//printf("buf2=%s  size=%d\n",buf2,size); //      ../../test.c  ->  ./test.c 1024
				for(i=strlen(buf2);i>0;i--)//就是把对方发来的文件都转换保存成自己执行的目录下
				{
					if(buf2[i]=='/')
					{	
						break;
					}
					
				}
				strcpy(buf3,&buf2[i]);
				sprintf(buf4,"./%s",buf3);
				
				if(ret==0)
				{
					break;
				}
				fd=open(buf4,O_RDWR|O_CREAT|O_TRUNC,0777);//创建文件--传来什么文件就保存什么文件
				if(fd<0)
				{
					perror("接收文件打开失败");
					continue;
				}
				while(1)
				{
					down_date=1;//传输的时候不允许退出Ctrl+C 都不可以
					read_file=read(newsocket,buf,sizeof(buf));//读
					if(read_file>0)
					{
						all_file+=read_file;
						write(fd,buf,read_file);   //写
						if(all_file==size)
						{
							printf("下载完成\n");
							down_date=0;
							close(fd);
							break;
						}
					}
					else
					{
						down_date=0;
						close(fd);
						break;
					}
					
					
				}
			}
			
		}
				
		else
		{
			//客户端掉线退出线程
			down_date=0;
			close(newsocket);
			pthread_exit(NULL);
		}

	}
}
	
//添加功能：1.不添加IP相同的用户    2.当用户退出的时候，把它从链表中删除，肯定要用户发送一个广播退出信息。

//信号处理函数 Ctrl+C
void sigfun(int arg)
{
	
	if(!down_date)
	{
		//发送广播数据	
		struct sockaddr_in addr;
	    memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_h);
		addr.sin_addr.s_addr = inet_addr("192.168.13.255");  //设置为广播地址
		sendto(socketfd,"exit", strlen("exit"), 0, (struct sockaddr *)&addr,sizeof(addr)); 
	
		printf("程序已经退出了\n");
		 
		// sleep(1);
		//退出整个进程 
		exit(0);
	}
	else
	{
		printf("文件传输中，不允许退出\n");
	}	
	
}
//信号处理函数 Ctrl+Z=Ctrl+C
void sigfun1(int arg)
{
	if(!down_date)
	{
		//发送广播数据	
		struct sockaddr_in addr;
	    memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_h);
		addr.sin_addr.s_addr = inet_addr("192.168.13.255");  //设置为广播地址
		sendto(socketfd,"exit", strlen("exit"), 0, (struct sockaddr *)&addr,sizeof(addr)); 
	
		printf("程序已经退出了\n");
		 
		// sleep(1);
		//退出整个进程 
		exit(0);
	}
	else
	{
		printf("文件传输中，不允许退出\n");
	}	
	
}

//除去上引号
void chage(char *zf)
{
	while(*(++zf)!='"')
	{
		printf("%c",*zf);
		
	
	}
}
//天气json http
int json_weather()
{
	char buf[1024]={0};
	char buf2[4096]={0};
	int  i=0,len=0;
	int wea=socket(AF_INET,SOCK_STREAM,0);
	if(wea<0)
	{
		printf("温度创建失败\n");
		return -1;
	}
	
	
	
	struct hostent *addr = gethostbyname("t.weather.sojson.com");
	if(addr == NULL)
	{
		perror("get ip failed");
		exit(1);
	}
	struct sockaddr_in weadther_addr={0};
	weadther_addr.sin_family = AF_INET;
	weadther_addr.sin_port = htons(80);	// 
	weadther_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)addr->h_addr_list[0]));
	
	int ret = connect(wea,(struct sockaddr *)&weadther_addr,sizeof(weadther_addr));
	if(ret<0)
	{
		printf("天气链接失败\n");
		return -1;
	}
	

	//http请求 :http://t.weather.sojson.com/api/weather/city/101280101
	strcpy(buf,"GET /api/weather/city/101280101 HTTP/1.1\r\nHost: t.weather.sojson.com\r\n\r\n");
	cJSON *weather_cjson=NULL;

	// 发送
/* 	write(wea,buf,strlen(buf));
	 int fd=open("./weather.txt",O_RDONLY);
	if(fd<0)
	{
		perror("没有weather.txt\n");
		return -1;
	} 
	// 接收
	read(fd,buf2,4096);
	
	
	 read(wea,buf2,4096); */
	// 处理
	
	while(weather_cjson==NULL)
	{
		
		sleep(1);
		bzero(buf2,4096);
		write(wea,buf,strlen(buf));
		read(wea,buf2,4096);
		   
		weather_cjson = cJSON_Parse(strstr(buf2,"{"));//转换成字符
		// printf("处理失败\n");
		// return -1;
	}
	// printf("\n%s\n",buf2);
	
	cJSON *value = cJSON_GetObjectItem(weather_cjson,"cityInfo");
	cJSON *value1 = value;
	
	value = cJSON_GetObjectItem(value1,"parent");
	char  *date=cJSON_Print(value);
	chage(date);
	 printf("省 ");
	
	
	value = cJSON_GetObjectItem(value1,"city");
	date=cJSON_Print(value);
	chage(date);
	// printf("%s ",date);
	
	value = cJSON_GetObjectItem(weather_cjson,"data");
	value1 = value;
	cJSON *value2 = value;
	
	value = cJSON_GetObjectItem(value,"wendu");
	date=cJSON_Print(value);
	chage(date);
	printf("℃  ");
	
	value = cJSON_GetObjectItem(value1,"quality");
	date=cJSON_Print(value);
	chage(date);
	printf(" 空气质量 ");
	
	value = cJSON_GetObjectItem(value1,"ganmao");
	date=cJSON_Print(value);
	chage(date);
	printf(" \n");
	
	printf("未来几天\n");
	
	
	value2 = cJSON_GetObjectItem(value2,"forecast");
	cJSON *new_value =NULL;
	len = cJSON_GetArraySize(value2);
	for(i=0;i<len;i++)
	{
		new_value=cJSON_GetArrayItem(value2,i);
		
		value = cJSON_GetObjectItem(new_value,"date");
		date=cJSON_Print(value);
		chage(date);
		printf(" ");
		
		
		value = cJSON_GetObjectItem(new_value,"high");
		date=cJSON_Print(value);
		chage(date);
		printf(" ");
		
		value = cJSON_GetObjectItem(new_value,"low");
		date=cJSON_Print(value);
		chage(date);
		printf("   ");
		
		value = cJSON_GetObjectItem(new_value,"notice");
		date=cJSON_Print(value);
		chage(date);
		printf(" \n");
		
	}
	close(wea);
}


int main()
{
	printf("\t**************************************************************\n");
	printf("\t*********************局域网通信程序精简版*********************\n");
	printf("\t**************************************************************\n");
				
	// printf("输入端口号\n");//端口
	// scanf("%d",&port_h);

	//创建UDP通讯
	socketfd = socket(AF_INET,SOCK_DGRAM,0);
	if(socketfd < 0 )
	{
		perror("创建UDP通讯失败");
		return -1;
	}
	//天气json
	json_weather();

	//设置UDP的地址并绑定 
	struct sockaddr_in  seraddr={0};
	seraddr.sin_family  = AF_INET; //使用IPv4协议
	seraddr.sin_port	= htons(port_h);   //网络通信都使用大端格式
	seraddr.sin_addr.s_addr =  inet_addr("0.0.0.0");  //32位的整形 
	int ret = bind(socketfd,(struct sockaddr*)&seraddr,sizeof(seraddr));//绑定
	if(ret < 0)
	{
		perror("绑定失败，检查下后台是否有在运行");
		return -1;
	}
	
	//自动获取广播IP
	struct ifreq gb={0};
	strcpy(gb.ifr_name,"ens33");
	ret = ioctl(socketfd,SIOCGIFBRDADDR/*获取广播IP的宏*/,&gb);
	if(ret<0)
	{
		bzero(&gb,sizeof(gb));
		strcpy(gb.ifr_name,"eth0");
		ret = ioctl(socketfd,SIOCGIFBRDADDR/*获取广播IP的宏*/,&gb);
		if(ret<0)
		{
			char aaa[10]={0};
			printf("广播IP获取失败 请输入你的网卡号,不是IP地址哦！\n");
			bzero(&gb,sizeof(gb));
			scanf("%s",aaa);
			strcpy(gb.ifr_name,aaa);
			ret = ioctl(socketfd,SIOCGIFBRDADDR/*获取广播IP的宏*/,&gb);
			if(ret<0)
			{
				printf("请在终端上输入ifconfig 查看网卡号哦");
				return -1;
			}
		}
		
		
	}
	
	
	
	
	

	//开启数据收发线程 
	pthread_t tid=0;
	pthread_create(&tid,NULL,func,NULL);
	pthread_t tid1=0;
	pthread_create(&tid1,NULL,func1,NULL);

	//开启发送广播数据  
	//2.开启发送广播数据功能
	int on=1;//开启
	ret = setsockopt(socketfd,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on));
	if(ret < 0)
	{
		perror("setsockopt fail\n");
		return -1;
	}

	//发送广播数据 
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_h);
	addr.sin_addr.s_addr = ((struct sockaddr_in*)&gb.ifr_addr)->sin_addr.s_addr;  //设置为广播地址

	sendto(socketfd,"join", strlen("join"), 0, (struct sockaddr *)&addr,sizeof(addr)); 


	//捕抓退出信号  如果在传输文件时候退出失败
	signal(SIGINT,sigfun);		//Ctrl+C 退出
	signal(SIGTSTP,sigfun1);		//Ctrl+Z=Ctrl+C
	
	
	
	//TCP  文件传输接收端
	int tcp_socket=socket(AF_INET,SOCK_STREAM,0);
	if(tcp_socket<0)
	{
		printf("创建TPC失败\n");
		return -1;
	}
	//设置端口复用
	
	ret = setsockopt(tcp_socket,SOL_SOCKET/*等级*/,SO_REUSEPORT/*属性*/,&on,sizeof(on));
	struct sockaddr_in  tcp_addr={0};
	tcp_addr.sin_family  = AF_INET; //使用IPv4协议
	tcp_addr.sin_port	= htons(port_h);   //网络通信都使用大端格式
	tcp_addr.sin_addr.s_addr =  inet_addr("0.0.0.0");  //32位的整形 

	int ret1=bind(tcp_socket,(struct sockaddr *)&tcp_addr,sizeof(tcp_addr));//绑定
	if(ret1<0)
	{
		printf("绑定失败\n");
		return -1;
	}
	ret1=listen(tcp_socket,5);//监听
	if(ret1<0)
	{
		printf("监听失败\n");
		return -1;
	}

		while(1)  //接待 TCP文件接收的
		{
			
			
			struct sockaddr_in clienaddr={0};
			int len=sizeof(clienaddr);
			int newsocket=0;
			while(1)
			{

				newsocket=accept(tcp_socket,(struct sockaddr *)&clienaddr,&len);//链接请求
				if(newsocket < 0)
				{
					perror("链接请求失败");
					return -1;
				}
				else
				{
					//输出对方的IP地址信息
					printf("TCP链接描述符=%d,发送文件者IP:%s,发送文件者端口:%d\n",newsocket,inet_ntoa(clienaddr.sin_addr),ntohs(clienaddr.sin_port));
					//创建线程去处理客户端的信息  
					pthread_t tid2;
					pthread_create(&tid2,NULL,func2,(void *)&newsocket);
				}
			}		


		}
	
	
}