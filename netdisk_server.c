#include <arpa/inet.h>
#include <fcntl.h>
#include <mysql/mysql.h>
#include <netinet/in.h>
#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  
#include <sys/socket.h>     
#include <time.h>

#define BUF_SIZE			1024 						//buf数组大小
#define FILE_BUF_SIZE		1024*256					//读文件的buf数组大小 256k
#define PORT 				20292						//端口号
#define APART				':'							//信息分隔符
#define APART2				'~'							//路径专用分隔符
#define LOGIN 				'1'							//登录操作
#define REGISTER			'0'							//注册操作
#define LOGINNOUSER			"2"							//登录-无此用户
#define LOGINRIGHT 			"1"							//登录-有此用户-密码正确
#define LOGINWRONG 			"0"							//登录-有此用户-密码错误
#define REGISTERSUCCESS		"1"							//注册成功
#define REGISTERFAIL		"0"							//注册失败-用户已存在
#define INFO				1							//消息日志标识
#define ERROR  				0							//错误日志标识
#define LISTENLEN			5							//监听队列长度
#define ORDERLEN			11							//client->server指令长度
//client->server
#define GETFILETREE 		"GETFILETREE" 				//申请获得文件树
#define UPLOADFILE			"UPLOAD_FILE"				//申请上传文件
#define DOWLOADFILE			"DOWNLODFILE"				//申请下载文件
#define COPYFILE			"COPYFILEEEE"				//申请复制文件
#define DELETEFILE			"DELETEFILEE"				//申请删除文件
#define ENDOFFILE 			"ENDOFFILEEE"				//文件上传结尾标志
#define RENAMEFILE			"RENAMEFILEE"				//申请重命名文件

//server->client
#define END_OF_FILETREE 	"THISISTHEENDOFFILETREE"	//文件树结尾标识	
#define UPLOADSTART			"UPLOADSTART"				//收到上传文件申请		
#define FILEHASEXIST		"FILEHASEXIST"				//文件已经存在，秒传 		
#define FILENOTEXIST		"FILENOTEXIST"				//文件不存在，正常上传
#define FILEZERO			"FILEZERO"					//零字节文件
#define UPLOADSUCCESS		"UPLOADSUCCESS"				//文件上传成功
#define COPYSUCCESS			"COPYSUCCESS"				//文件复制成功
#define DELETESUCCESS 		"DELETESUCCESS"				//文件删除成功



//------------------------------------------工具函数------------------------------------------

/*将数组a的第begin->end个字符写入数组b*/
void swap(char a[], char b[], int begin, int end) {
	int i;
	for (i = begin; i <= end; i++)
		b[i - begin] = a[i];
	b[end - begin + 1] = '\0';
}

/*写日志*/
void writelog(int msgtype, char* msg) {
	time_t now;
	struct tm* t;
	time(&now);//获取当前时间
	t = localtime(&now);
	t->tm_hour = (t->tm_hour) % 24;
	t->tm_min = (t->tm_min) % 60;
	int fd;
	fd = open("netdisk_server.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
	char time[30];
	sprintf(time, "%d-%02d-%02d %02d:%02d:%02d    ", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	write(fd, time, 23);
	if (msgtype == INFO)
		write(fd, "INFO    ", 8);
	else if (msgtype == ERROR)
		write(fd, "ERROR   ", 8);
	write(fd, msg, strlen(msg));
	write(fd, "\n", 1);
}

/*写错误日志并退出*/
void errorlog(char* msg) {
	writelog(ERROR, msg);
	exit(1);
}

/* 数据库查询，只在有错误时写日志，成功时视情况自己写*/
void myquery(MYSQL* mysql, char msg[]) {
	//printf("strlen(msg)=%d",strlen(msg));
	if (mysql_real_query(mysql, msg, strlen(msg))) {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "mysql_real_connect failed : %s", mysql_error(mysql));
		//printf("%s\n", msg);
		errorlog(msg);
	}
}

/*用于将得到的字节数字符数组变成int*/
int getNum(char* str) {
	int num = 0, i = 0;
	while (str[i] != '\0')
	{
		num = num * 10 + str[i] - '0';
		i++;
	}
	return num;
}

/*删除操作(文件夹，未实装)*/
void deletefile(MYSQL* mysql, char username[], char id[]) {
	char msg[100];
	MYSQL_ROW  row;
	int isfile = 0;
	char md5[50];
	char fileid[50];
	memset(md5, 0, sizeof(md5));
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "select*from %s where usrfile_id='%s'", username, id);
	myquery(mysql, msg);

	MYSQL_RES* result = mysql_store_result(mysql);
	while ((row = mysql_fetch_row(result)) != NULL) {
		if (strcmp(row[4], "1")) {
			sprintf(md5, "%s", row[2]);
			isfile = 1;
		}
	}
	printf("isfile=%d\n", isfile);
	mysql_free_result(result);

	memset(msg, 0, sizeof(msg));
	sprintf(msg, "delete from %s where usrfile_id='%s'", username, id);
	myquery(mysql, msg);
	printf("delete over\n", isfile);
	if (isfile) {
		//文件链接使用数-1
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "update file set file_usenum=file_usenum-1 where file_name='%s'", md5);
		myquery(mysql, msg);
	}

	if (!isfile) {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "select*from %s where usrfile_fatherid='%s'", username, id);
		myquery(mysql, msg);
		MYSQL_RES* result1 = mysql_store_result(mysql);
		while ((row = mysql_fetch_row(result1)) != NULL) {
			memset(fileid, 0, sizeof(fileid));
			sprintf(fileid, "%s", row[0]);
			deletefile(mysql, username, fileid);
		}
		mysql_free_result(result1);
	}
}

/*连接*/
int connect123(int server_sockfd, char* username, MYSQL* mysql) {
	//附属定义区
	int client_sockfd;
	struct sockaddr_in client_addr;
	char msg[BUF_SIZE];
	char buf[BUF_SIZE];
	int sin_size;
	int loginover = 1;
	int islogin;
	int nlen;
	int i, j;
	char pwd[50];
	int islogincorrect;
	MYSQL_ROW  row;


	/*监听连接请求--监听队列长度为5*/
	listen(server_sockfd, LISTENLEN);
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "listen length : %d", LISTENLEN);
	writelog(INFO, msg);

	/*等待客户端连接请求到达*/
	sin_size = sizeof(struct sockaddr_in);
	if ((client_sockfd = accept(server_sockfd, (struct sockaddr*) & client_addr, &sin_size)) < 0)
		errorlog("accept");
	else {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "accept ok , client ip:%s", inet_ntoa(client_addr.sin_addr));
		writelog(INFO, msg);
		printf("%s\n", msg);
	}

	while (loginover) {
		islogin = 1;						//将islogin重置
		printf("waiting for login/register\n");
		/*buf第一位：1登录 0注册，2到：之前用户名，：到：之间密码*/
		memset(buf, 0, sizeof(buf));
		nlen = read(client_sockfd, buf, BUF_SIZE);
		if (nlen <= 0)//client端退出
		{
			mysql_close(mysql);
			close(client_sockfd);
			close(server_sockfd);
			writelog(ERROR, "client exit");
			exit(1);
		}
		buf[nlen] = '\0';					//结尾写sb尾零
		if (buf[0] == REGISTER)			//注册
			islogin = 0;

		/*用户名密码数据分离处理*/
		for (i = 1; i < strlen(buf); i++) {
			if (buf[i] == APART)
				break;
		}
		for (j = i + 1; j < strlen(buf); j++) {
			if (buf[j] == APART)
				break;
		}
		swap(buf, username, 1, i - 1);		//将用户名信息写到username里
		swap(buf, pwd, i + 1, j - 1);			//将密码信息写到pwd里
		username[i] = '\0';

		//登录
		if (islogin) {
			islogincorrect = 0;
			/*先检查是否有此用户*/
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "SELECT*FROM user WHERE user_name='%s'", username);
			myquery(mysql, msg);

			MYSQL_RES* result = mysql_store_result(mysql);
			row = mysql_fetch_row(result);

			if (row == NULL) {
				writelog(ERROR, "no such user");
				write(client_sockfd, LOGINNOUSER, 1);//通知client端无此用户
				//printf("no such user\n");
				mysql_free_result(result);
				continue;//进入下一个循环等待重新发送信息
			}
			mysql_free_result(result);

			/*再验证密码正确性*/
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "SELECT*FROM user WHERE user_pwd=md5('%s')", pwd);
			myquery(mysql, msg);
			MYSQL_RES* result2 = mysql_store_result(mysql);
			while ((row = mysql_fetch_row(result2)) != NULL) {

				//密码错误
				if (row == NULL) {
					//printf("wrong pwd\n");
					write(client_sockfd, LOGINWRONG, 1);//通知client端密码错误
					writelog(ERROR, "wrong password");
					mysql_free_result(result2);
					break;
				}

				//密码正确
				else if (!strcmp(row[0], username)) {
					writelog(INFO, "login success");
					mysql_free_result(result2);
					loginover = 0;						//loginover置零，登录过程结束
					islogincorrect = 1;
					break;
				}
			}
			if (!islogincorrect) {
				write(client_sockfd, LOGINWRONG, 1);//通知client端密码错误
				writelog(INFO, "wrong password");
				mysql_free_result(result2);
			}

		}
		//注册
		else {
			//直接进行数据库插入工作，若用户名已存在则会返回错误
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "INSERT INTO user VALUES('%s',md5('%s'))", username, pwd);
			if (mysql_query(mysql, msg)) {
				write(client_sockfd, REGISTERFAIL, 1);//通知client端注册失败，用户已存在
				writelog(ERROR, "user already exist");
			}
			else {
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "register new user : %s", username);
				writelog(INFO, msg);
				//创建用户文件表
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "DROP TABLE IF EXISTS `%s`", username);
				myquery(mysql, msg);
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "CREATE TABLE `%s`(`usrfile_id` INT(11) NOT NULL AUTO_INCREMENT,`usrfile_name` VARCHAR(128) NOT NULL,", username);
				sprintf(msg, "%s`usrfile_hashname` VARCHAR(128),`usrfile_fatherid` INT(11),`usrfile_isfile` TINYINT(1) NOT NULL,", msg);
				sprintf(msg, "%sPRIMARY KEY(`usrfile_id`)) ENGINE=InnoDB DEFAULT CHARSET=gbk AUTO_INCREMENT=1;", msg);
				myquery(mysql, msg);
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'root',NULL,NULL,0)", username);
				myquery(mysql, msg);
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'test',NULL,1,0)", username);
				myquery(mysql, msg);
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "create table for new user : %s", username);
				writelog(INFO, msg);
				write(client_sockfd, REGISTERSUCCESS, 1);	//通知client端注册成功，接下来下一个循环进行登录
			}
		}
	}
	return client_sockfd;
}




int main() {
	//------------------------------------------定义区------------------------------------------ 
	int server_sockfd;				//服务器端套接字  
	int client_sockfd;				//客户端套接字  
	int nlen;  						//read write返回值
	struct sockaddr_in server_addr; //服务器网络地址结构体  
	//struct sockaddr_in client_addr; //客户端网络地址结构体 


	char buf[BUF_SIZE]; 			//数据传送的缓冲区
	int i, j;						//工具人
	char msg[BUF_SIZE];				//BUF_SIZE大小的char数组工具人
	char filebuf[FILE_BUF_SIZE + 1];	//读文件时所使用的buf
	//char file[FILE_BUF_SIZE];
	char* username;				//用户名
	username = (char*)malloc(sizeof(char) * 50);
	//char pwd[50];					//密码
	char order[20];					//client传来命令
	//usrfile专区
	char fileid[10];				//文件id
	char fatherid[10];				//父目录id
	char md5[50];					//文件Md5码
	char filename[50];				//文件真实名称
	char filesizech[20];			//文件大小
	int filesize = 0;					//文件大小
	int filep = 0;
	char filedir[50];				//文件路径
	//flag专区				
	int islogin = 1;				//1登录 0注册
	int isover = 1;					//是否client还在骚扰我
	int isuploadover = 0;				//是否文件上传完毕
	int islogincorrect = 0;			//密码是否正确
	//数据库专区
	MYSQL* mysql;  					//数据库套接字（大概）
	MYSQL_ROW  row;					//读取数据库返回内容




//-------------------------------------数据库初始化及连接----------------------------------------

	/* 初始化 mysql 变量，失败返回NULL */
	if ((mysql = mysql_init(NULL)) == NULL) {
		printf("mysql_init failed\n");
		errorlog("mysql init");
	}
	else
	{
		//printf("mysql init ok\n");
		writelog(INFO, "mysql init success");
	}

	/* 连接数据库，失败返回NULL 可能原因：1、mysqld没运行 2、没有指定名称的数据库存在 */
	if (mysql_real_connect(mysql, "localhost", "u1751561", "u1751561", "db1751561", 0, NULL, 0) == NULL) {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "mysql_real_connect failed : %s", mysql_error(mysql));
		//printf("%s\n", msg);
		errorlog(msg);
	}
	else {
		//printf("mysql connect ok\n");
		writelog(INFO, "mysql connect success");
	}

	/* 设置字符集，否则读出的字符乱码，即使/etc/my.cnf中设置也不行 */
	mysql_set_character_set(mysql, "gbk");




	//------------------------------------------tcp连接------------------------------------------

	memset(&server_addr, 0, sizeof(server_addr)); //数据初始化--清零  
	server_addr.sin_family = AF_INET; //设置为IP通信  
	server_addr.sin_addr.s_addr = INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上    
	server_addr.sin_port = htons(PORT); //服务器端口号 
	/*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
	if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		errorlog("socket");
	else
		writelog(INFO, "socket create");

	/*将套接字绑定到服务器的网络地址上*/
	if (bind(server_sockfd, (struct sockaddr*) & server_addr, sizeof(struct sockaddr)) < 0)
		errorlog("bind");
	else {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "bind with port : %d", PORT);
		writelog(INFO, msg);
	}
	client_sockfd = connect123(server_sockfd, username, mysql);
	write(client_sockfd, "1~0~0~", 5);//通知client端密码正确




//------------------------------------------正片开始------------------------------------------
	int rollnum = 0;
	while (isover) {
		rollnum++;
		printf("第%d轮\n", rollnum);
		printf("waiting for client request\n");
		memset(buf, 0, sizeof(buf));
		nlen = read(client_sockfd, buf, BUF_SIZE);//client端发来请求

		if (nlen <= 0)//client端退出
		{
			mysql_close(mysql);
			close(client_sockfd);
			close(server_sockfd);
			errorlog("client exit");
		}
		buf[nlen] = '\0';
		printf("buf=%s\n", buf);


		memset(order, '\0', sizeof(order));
		strncpy(order, buf, ORDERLEN);		//截取前ORDERLEN位
		//printf("order=%s\n",order);

		/*----1、发送用户文件表----*/
		if (!strcmp(order, GETFILETREE)) {
			writelog(INFO, "client asks for filetree");

			memset(msg, 0, sizeof(msg));
			sprintf(msg, "SELECT*FROM %s", username);
			myquery(mysql, msg);

			memset(msg, 0, sizeof(msg));
			MYSQL_RES* result3 = mysql_store_result(mysql);
			while ((row = mysql_fetch_row(result3)) != NULL) {
				if (row[3] == NULL)
					sprintf(msg, "%s%s:%s:%s:%s:", msg, row[0], row[1], "0", row[4]);
				else
					sprintf(msg, "%s%s:%s:%s:%s:", msg, row[0], row[1], row[3], row[4]);
			}
			mysql_free_result(result3);

			sprintf(msg, "%s%s:", msg, END_OF_FILETREE);
			//printf("%s\n",msg);
			write(client_sockfd, msg, strlen(msg));
			writelog(INFO, "send filetree success");
		}

		/*----2、上传文件----*/
		/*buf结构：UPLOADFILE:父目录id:md5码:真实名称:大小:绝对路径~*/
		else if (!strcmp(order, UPLOADFILE)) {
			writelog(INFO, "client request UPLOAD file");
			memset(fatherid, '\0', sizeof(fatherid));
			memset(md5, '\0', sizeof(md5));
			memset(filename, '\0', sizeof(filename));
			memset(filesizech, '\0', sizeof(filesizech));
			memset(filedir, '\0', sizeof(filedir));
			for (i = ORDERLEN + 1; i < strlen(buf); i++) {
				if (buf[i] == APART)
					break;
			}
			swap(buf, fatherid, ORDERLEN + 1, i - 1);
			for (j = i + 1; j < strlen(buf); j++) {
				if (buf[j] == APART)
					break;
			}
			swap(buf, md5, i + 1, j - 1);
			for (i = j + 1; i < strlen(buf); i++) {
				if (buf[i] == APART)
					break;
			}
			swap(buf, filename, j + 1, i - 1);
			for (j = i + 1; j < strlen(buf); j++) {
				if (buf[j] == APART)
					break;
			}
			swap(buf, filesizech, i + 1, j - 1);
			for (i = j + 1; i < strlen(buf); i++) {
				if (buf[i] == APART2)
					break;
			}
			swap(buf, filedir, j + 1, i - 1);

			//printf("fatherid=%s\n",fatherid);
			//printf("md5=%s\n",md5);
			//printf("filename=%s\n",filename);
			//printf("filesizech=%s\n",filesizech);
			filesize = getNum(filesizech);
			//printf("filesize=%d\n",filesize);
			//printf("filedir=%s\n",filedir);
			//尝试将文件插入file表中，若失败则说明已存在，直接返回秒传标志
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "INSERT INTO file VALUES('%s',DEFAULT)", md5);
			if (mysql_query(mysql, msg)) {
				write(client_sockfd, FILEHASEXIST, strlen(FILEHASEXIST));//通知client端文件已存在，秒传
				writelog(INFO, "file already exist , flashtrans");
				//文件链接使用数+1
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "update file set file_usenum=file_usenum+1 where file_name='%s'", md5);
				myquery(mysql, msg);
				//写进用户文件表
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'%s','%s','%s',1)", username, filename, md5, fatherid);
				//printf("%s\n",msg);
				myquery(mysql, msg);
			}
			else {
				write(client_sockfd, FILENOTEXIST, strlen(FILENOTEXIST));//通知client端文件不存在，开始上传
				//开始读client端发过来的文件内容
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "./file/%s", md5);
				//remove(msg);
				int fd = open(msg, O_WRONLY | O_CREAT | O_APPEND, 0777);//创建文件
				if (filesize != 0) {
					int uploadnum = 0;
					int isuploadover = 0;
					//int aaa=0;
					while (1) {
						memset(filebuf, 0, FILE_BUF_SIZE);
						int filep = 0;

						while (1)
						{
							nlen = read(client_sockfd, filebuf + filep, FILE_BUF_SIZE - filep);
							//断点重传
							if (nlen <= 0) {
								writelog(INFO, "client stop file transfer");
								client_sockfd = connect123(server_sockfd, username, mysql);
								memset(filebuf, 0, FILE_BUF_SIZE);
								filep = 0;
								memset(msg, 0, sizeof(msg));
								sprintf(msg, "%s~%d~%s~", LOGINRIGHT, uploadnum, filedir);
								nlen = 0;
								write(client_sockfd, msg, strlen(msg));
							}
							//printf("1\n");
							filep += nlen;
							//printf("filep=%d\n",filep);
							if (filep == filesize) {
								isuploadover = 1;
								break;
							}
							if (filep == FILE_BUF_SIZE) {
								filesize -= FILE_BUF_SIZE;
								break;
							}
						}
						if (isuploadover) {//最后一个
							write(fd, filebuf, filesize);
							//write(fd,'\0',1);
							uploadnum++;
							printf("最后一个包上传成功\n");
							break;
						}
						write(client_sockfd, UPLOADSUCCESS, strlen(UPLOADSUCCESS));//通知client上传成功
						write(fd, filebuf, FILE_BUF_SIZE);
						uploadnum++;
						printf("第%d个包上传成功\n", uploadnum);
					}
				}

				close(fd);
				//文件读取结束后再写进用户表
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'%s','%s','%s',1)", username, filename, md5, fatherid);
				//printf("%s\n",msg);
				myquery(mysql, msg);
				if (filesize == 0)
					writelog(INFO, "zero byte file upload success");
				else
					writelog(INFO, "file upload success");
				write(client_sockfd, UPLOADSUCCESS, strlen(UPLOADSUCCESS));//通知client上传成功
			}
		}

		/*----3、复制文件，和秒传相似----*/
		/*buf结构->COPYFILE:要复制到的文件夹id:文件id:*/
		else if (!strcmp(order, COPYFILE)) {
			writelog(INFO, "client request COPY file");
			memset(fatherid, '\0', sizeof(fatherid));
			memset(fileid, '\0', sizeof(fileid));
			for (i = ORDERLEN + 1; i < strlen(buf); i++) {
				if (buf[i] == APART)
					break;
			}
			swap(buf, fatherid, ORDERLEN + 1, i - 1);
			for (j = i + 1; j < strlen(buf); j++) {
				if (buf[j] == APART)
					break;
			}
			swap(buf, fileid, i + 1, j - 1);

			memset(msg, 0, sizeof(msg));
			sprintf(msg, "select*from %s where usrfile_id='%s'", username, fileid);
			myquery(mysql, msg);
			MYSQL_RES* result4 = mysql_store_result(mysql);
			while ((row = mysql_fetch_row(result4)) != NULL) {
				memset(filename, 0, sizeof(filename));
				memset(md5, 0, sizeof(md5));
				//复制到当前文件夹下，文件名前加上bak-
				if (!strcmp(row[3], fatherid))
					sprintf(filename, "bak-%s", row[1]);
				else
					sprintf(filename, "%s", row[1]);
				sprintf(md5, "%s", row[2]);
			}
			mysql_free_result(result4);
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "insert into %s values(NULL,'%s','%s','%s',1)", username, filename, md5, fatherid);
			myquery(mysql, msg);

			//文件链接使用数+1
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "update file set file_usenum=file_usenum+1 where file_name='%s'", md5);
			myquery(mysql, msg);
			writelog(INFO, "copy success");
			//write(client_sockfd,COPYSUCCESS,strlen(COPYSUCCESS));//向client端发送复制成功
		}

		/*----4、删除文件----*/
		/*buf结构->DELETEFILE:文件id:*/
		else if (!strcmp(order, DELETEFILE)) {
			writelog(INFO, "client request DELETE file");
			memset(fileid, '\0', sizeof(fileid));
			for (i = ORDERLEN + 1; i < strlen(buf); i++) {
				if (buf[i] == APART)
					break;
			}
			//printf("delete\n");
			swap(buf, fileid, ORDERLEN + 1, i - 1);

			memset(msg, 0, sizeof(msg));
			sprintf(msg, "select*from %s where usrfile_id='%s'", username, fileid);
			myquery(mysql, msg);
			MYSQL_RES* result5 = mysql_store_result(mysql);
			while ((row = mysql_fetch_row(result5)) != NULL) {
				memset(md5, 0, sizeof(md5));
				sprintf(md5, "%s", row[2]);
			}
			mysql_free_result(result5);

			memset(msg, 0, sizeof(msg));
			sprintf(msg, "delete from %s where usrfile_id='%s'", username, fileid);
			myquery(mysql, msg);

			//文件链接使用数-1
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "update file set file_usenum=file_usenum-1 where file_name='%s'", md5);
			myquery(mysql, msg);
			writelog(INFO, "delete success");
			//write(client_sockfd,DELETESUCCESS,strlen(DELETESUCCESS));//向client端发送删除成功
		}

		/*------5、重命名------*/
		/*buf结构->RENAMEFILE:文件id:改后名字:*/
		else if (!strcmp(order, RENAMEFILE)) {
			writelog(INFO, "client request RENAME file");
			memset(fileid, '\0', sizeof(fileid));
			memset(filedir, '\0', sizeof(filedir));
			for (i = ORDERLEN + 1; i < strlen(buf); i++) {
				if (buf[i] == APART)
					break;
			}
			swap(buf, fileid, ORDERLEN + 1, i - 1);
			for (j = i + 1; j < strlen(buf); j++) {
				if (buf[j] == APART)
					break;
			}
			swap(buf, filedir, i + 1, j - 1);
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "update %s set usrfile_name='%s' where usrfile_id='%s'", username, filedir, fileid);
			myquery(mysql, msg);

			writelog(INFO, "rename success");
			//write(client_sockfd,DELETESUCCESS,strlen(RENAMESUCCESS));//向client端发送删除成功
		}

	}




	//------------------------------------------善后处理工作------------------------------------------
	mysql_close(mysql);
	close(client_sockfd);
	close(server_sockfd);
	return 0;
}