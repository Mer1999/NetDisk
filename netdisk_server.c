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

#define BUF_SIZE			1024 						//buf�����С
#define FILE_BUF_SIZE		1024*256					//���ļ���buf�����С 256k
#define PORT 				20292						//�˿ں�
#define APART				':'							//��Ϣ�ָ���
#define APART2				'~'							//·��ר�÷ָ���
#define LOGIN 				'1'							//��¼����
#define REGISTER			'0'							//ע�����
#define LOGINNOUSER			"2"							//��¼-�޴��û�
#define LOGINRIGHT 			"1"							//��¼-�д��û�-������ȷ
#define LOGINWRONG 			"0"							//��¼-�д��û�-�������
#define REGISTERSUCCESS		"1"							//ע��ɹ�
#define REGISTERFAIL		"0"							//ע��ʧ��-�û��Ѵ���
#define INFO				1							//��Ϣ��־��ʶ
#define ERROR  				0							//������־��ʶ
#define LISTENLEN			5							//�������г���
#define ORDERLEN			11							//client->serverָ���
//client->server
#define GETFILETREE 		"GETFILETREE" 				//�������ļ���
#define UPLOADFILE			"UPLOAD_FILE"				//�����ϴ��ļ�
#define DOWLOADFILE			"DOWNLODFILE"				//���������ļ�
#define COPYFILE			"COPYFILEEEE"				//���븴���ļ�
#define DELETEFILE			"DELETEFILEE"				//����ɾ���ļ�
#define ENDOFFILE 			"ENDOFFILEEE"				//�ļ��ϴ���β��־
#define RENAMEFILE			"RENAMEFILEE"				//�����������ļ�

//server->client
#define END_OF_FILETREE 	"THISISTHEENDOFFILETREE"	//�ļ�����β��ʶ	
#define UPLOADSTART			"UPLOADSTART"				//�յ��ϴ��ļ�����		
#define FILEHASEXIST		"FILEHASEXIST"				//�ļ��Ѿ����ڣ��봫 		
#define FILENOTEXIST		"FILENOTEXIST"				//�ļ������ڣ������ϴ�
#define FILEZERO			"FILEZERO"					//���ֽ��ļ�
#define UPLOADSUCCESS		"UPLOADSUCCESS"				//�ļ��ϴ��ɹ�
#define COPYSUCCESS			"COPYSUCCESS"				//�ļ����Ƴɹ�
#define DELETESUCCESS 		"DELETESUCCESS"				//�ļ�ɾ���ɹ�



//------------------------------------------���ߺ���------------------------------------------

/*������a�ĵ�begin->end���ַ�д������b*/
void swap(char a[], char b[], int begin, int end) {
	int i;
	for (i = begin; i <= end; i++)
		b[i - begin] = a[i];
	b[end - begin + 1] = '\0';
}

/*д��־*/
void writelog(int msgtype, char* msg) {
	time_t now;
	struct tm* t;
	time(&now);//��ȡ��ǰʱ��
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

/*д������־���˳�*/
void errorlog(char* msg) {
	writelog(ERROR, msg);
	exit(1);
}

/* ���ݿ��ѯ��ֻ���д���ʱд��־���ɹ�ʱ������Լ�д*/
void myquery(MYSQL* mysql, char msg[]) {
	//printf("strlen(msg)=%d",strlen(msg));
	if (mysql_real_query(mysql, msg, strlen(msg))) {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "mysql_real_connect failed : %s", mysql_error(mysql));
		//printf("%s\n", msg);
		errorlog(msg);
	}
}

/*���ڽ��õ����ֽ����ַ�������int*/
int getNum(char* str) {
	int num = 0, i = 0;
	while (str[i] != '\0')
	{
		num = num * 10 + str[i] - '0';
		i++;
	}
	return num;
}

/*ɾ������(�ļ��У�δʵװ)*/
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
		//�ļ�����ʹ����-1
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

/*����*/
int connect123(int server_sockfd, char* username, MYSQL* mysql) {
	//����������
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


	/*������������--�������г���Ϊ5*/
	listen(server_sockfd, LISTENLEN);
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "listen length : %d", LISTENLEN);
	writelog(INFO, msg);

	/*�ȴ��ͻ����������󵽴�*/
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
		islogin = 1;						//��islogin����
		printf("waiting for login/register\n");
		/*buf��һλ��1��¼ 0ע�ᣬ2����֮ǰ�û�����������֮������*/
		memset(buf, 0, sizeof(buf));
		nlen = read(client_sockfd, buf, BUF_SIZE);
		if (nlen <= 0)//client���˳�
		{
			mysql_close(mysql);
			close(client_sockfd);
			close(server_sockfd);
			writelog(ERROR, "client exit");
			exit(1);
		}
		buf[nlen] = '\0';					//��βдsbβ��
		if (buf[0] == REGISTER)			//ע��
			islogin = 0;

		/*�û����������ݷ��봦��*/
		for (i = 1; i < strlen(buf); i++) {
			if (buf[i] == APART)
				break;
		}
		for (j = i + 1; j < strlen(buf); j++) {
			if (buf[j] == APART)
				break;
		}
		swap(buf, username, 1, i - 1);		//���û�����Ϣд��username��
		swap(buf, pwd, i + 1, j - 1);			//��������Ϣд��pwd��
		username[i] = '\0';

		//��¼
		if (islogin) {
			islogincorrect = 0;
			/*�ȼ���Ƿ��д��û�*/
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "SELECT*FROM user WHERE user_name='%s'", username);
			myquery(mysql, msg);

			MYSQL_RES* result = mysql_store_result(mysql);
			row = mysql_fetch_row(result);

			if (row == NULL) {
				writelog(ERROR, "no such user");
				write(client_sockfd, LOGINNOUSER, 1);//֪ͨclient���޴��û�
				//printf("no such user\n");
				mysql_free_result(result);
				continue;//������һ��ѭ���ȴ����·�����Ϣ
			}
			mysql_free_result(result);

			/*����֤������ȷ��*/
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "SELECT*FROM user WHERE user_pwd=md5('%s')", pwd);
			myquery(mysql, msg);
			MYSQL_RES* result2 = mysql_store_result(mysql);
			while ((row = mysql_fetch_row(result2)) != NULL) {

				//�������
				if (row == NULL) {
					//printf("wrong pwd\n");
					write(client_sockfd, LOGINWRONG, 1);//֪ͨclient���������
					writelog(ERROR, "wrong password");
					mysql_free_result(result2);
					break;
				}

				//������ȷ
				else if (!strcmp(row[0], username)) {
					writelog(INFO, "login success");
					mysql_free_result(result2);
					loginover = 0;						//loginover���㣬��¼���̽���
					islogincorrect = 1;
					break;
				}
			}
			if (!islogincorrect) {
				write(client_sockfd, LOGINWRONG, 1);//֪ͨclient���������
				writelog(INFO, "wrong password");
				mysql_free_result(result2);
			}

		}
		//ע��
		else {
			//ֱ�ӽ������ݿ���빤�������û����Ѵ�����᷵�ش���
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "INSERT INTO user VALUES('%s',md5('%s'))", username, pwd);
			if (mysql_query(mysql, msg)) {
				write(client_sockfd, REGISTERFAIL, 1);//֪ͨclient��ע��ʧ�ܣ��û��Ѵ���
				writelog(ERROR, "user already exist");
			}
			else {
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "register new user : %s", username);
				writelog(INFO, msg);
				//�����û��ļ���
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
				write(client_sockfd, REGISTERSUCCESS, 1);	//֪ͨclient��ע��ɹ�����������һ��ѭ�����е�¼
			}
		}
	}
	return client_sockfd;
}




int main() {
	//------------------------------------------������------------------------------------------ 
	int server_sockfd;				//���������׽���  
	int client_sockfd;				//�ͻ����׽���  
	int nlen;  						//read write����ֵ
	struct sockaddr_in server_addr; //�����������ַ�ṹ��  
	//struct sockaddr_in client_addr; //�ͻ��������ַ�ṹ�� 


	char buf[BUF_SIZE]; 			//���ݴ��͵Ļ�����
	int i, j;						//������
	char msg[BUF_SIZE];				//BUF_SIZE��С��char���鹤����
	char filebuf[FILE_BUF_SIZE + 1];	//���ļ�ʱ��ʹ�õ�buf
	//char file[FILE_BUF_SIZE];
	char* username;				//�û���
	username = (char*)malloc(sizeof(char) * 50);
	//char pwd[50];					//����
	char order[20];					//client��������
	//usrfileר��
	char fileid[10];				//�ļ�id
	char fatherid[10];				//��Ŀ¼id
	char md5[50];					//�ļ�Md5��
	char filename[50];				//�ļ���ʵ����
	char filesizech[20];			//�ļ���С
	int filesize = 0;					//�ļ���С
	int filep = 0;
	char filedir[50];				//�ļ�·��
	//flagר��				
	int islogin = 1;				//1��¼ 0ע��
	int isover = 1;					//�Ƿ�client����ɧ����
	int isuploadover = 0;				//�Ƿ��ļ��ϴ����
	int islogincorrect = 0;			//�����Ƿ���ȷ
	//���ݿ�ר��
	MYSQL* mysql;  					//���ݿ��׽��֣���ţ�
	MYSQL_ROW  row;					//��ȡ���ݿⷵ������




//-------------------------------------���ݿ��ʼ��������----------------------------------------

	/* ��ʼ�� mysql ������ʧ�ܷ���NULL */
	if ((mysql = mysql_init(NULL)) == NULL) {
		printf("mysql_init failed\n");
		errorlog("mysql init");
	}
	else
	{
		//printf("mysql init ok\n");
		writelog(INFO, "mysql init success");
	}

	/* �������ݿ⣬ʧ�ܷ���NULL ����ԭ��1��mysqldû���� 2��û��ָ�����Ƶ����ݿ���� */
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

	/* �����ַ���������������ַ����룬��ʹ/etc/my.cnf������Ҳ���� */
	mysql_set_character_set(mysql, "gbk");




	//------------------------------------------tcp����------------------------------------------

	memset(&server_addr, 0, sizeof(server_addr)); //���ݳ�ʼ��--����  
	server_addr.sin_family = AF_INET; //����ΪIPͨ��  
	server_addr.sin_addr.s_addr = INADDR_ANY;//������IP��ַ--�������ӵ����б��ص�ַ��    
	server_addr.sin_port = htons(PORT); //�������˿ں� 
	/*�������������׽���--IPv4Э�飬��������ͨ�ţ�TCPЭ��*/
	if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		errorlog("socket");
	else
		writelog(INFO, "socket create");

	/*���׽��ְ󶨵��������������ַ��*/
	if (bind(server_sockfd, (struct sockaddr*) & server_addr, sizeof(struct sockaddr)) < 0)
		errorlog("bind");
	else {
		memset(msg, 0, sizeof(msg));
		sprintf(msg, "bind with port : %d", PORT);
		writelog(INFO, msg);
	}
	client_sockfd = connect123(server_sockfd, username, mysql);
	write(client_sockfd, "1~0~0~", 5);//֪ͨclient��������ȷ




//------------------------------------------��Ƭ��ʼ------------------------------------------
	int rollnum = 0;
	while (isover) {
		rollnum++;
		printf("��%d��\n", rollnum);
		printf("waiting for client request\n");
		memset(buf, 0, sizeof(buf));
		nlen = read(client_sockfd, buf, BUF_SIZE);//client�˷�������

		if (nlen <= 0)//client���˳�
		{
			mysql_close(mysql);
			close(client_sockfd);
			close(server_sockfd);
			errorlog("client exit");
		}
		buf[nlen] = '\0';
		printf("buf=%s\n", buf);


		memset(order, '\0', sizeof(order));
		strncpy(order, buf, ORDERLEN);		//��ȡǰORDERLENλ
		//printf("order=%s\n",order);

		/*----1�������û��ļ���----*/
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

		/*----2���ϴ��ļ�----*/
		/*buf�ṹ��UPLOADFILE:��Ŀ¼id:md5��:��ʵ����:��С:����·��~*/
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
			//���Խ��ļ�����file���У���ʧ����˵���Ѵ��ڣ�ֱ�ӷ����봫��־
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "INSERT INTO file VALUES('%s',DEFAULT)", md5);
			if (mysql_query(mysql, msg)) {
				write(client_sockfd, FILEHASEXIST, strlen(FILEHASEXIST));//֪ͨclient���ļ��Ѵ��ڣ��봫
				writelog(INFO, "file already exist , flashtrans");
				//�ļ�����ʹ����+1
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "update file set file_usenum=file_usenum+1 where file_name='%s'", md5);
				myquery(mysql, msg);
				//д���û��ļ���
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'%s','%s','%s',1)", username, filename, md5, fatherid);
				//printf("%s\n",msg);
				myquery(mysql, msg);
			}
			else {
				write(client_sockfd, FILENOTEXIST, strlen(FILENOTEXIST));//֪ͨclient���ļ������ڣ���ʼ�ϴ�
				//��ʼ��client�˷��������ļ�����
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "./file/%s", md5);
				//remove(msg);
				int fd = open(msg, O_WRONLY | O_CREAT | O_APPEND, 0777);//�����ļ�
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
							//�ϵ��ش�
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
						if (isuploadover) {//���һ��
							write(fd, filebuf, filesize);
							//write(fd,'\0',1);
							uploadnum++;
							printf("���һ�����ϴ��ɹ�\n");
							break;
						}
						write(client_sockfd, UPLOADSUCCESS, strlen(UPLOADSUCCESS));//֪ͨclient�ϴ��ɹ�
						write(fd, filebuf, FILE_BUF_SIZE);
						uploadnum++;
						printf("��%d�����ϴ��ɹ�\n", uploadnum);
					}
				}

				close(fd);
				//�ļ���ȡ��������д���û���
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "insert into %s values(NULL,'%s','%s','%s',1)", username, filename, md5, fatherid);
				//printf("%s\n",msg);
				myquery(mysql, msg);
				if (filesize == 0)
					writelog(INFO, "zero byte file upload success");
				else
					writelog(INFO, "file upload success");
				write(client_sockfd, UPLOADSUCCESS, strlen(UPLOADSUCCESS));//֪ͨclient�ϴ��ɹ�
			}
		}

		/*----3�������ļ������봫����----*/
		/*buf�ṹ->COPYFILE:Ҫ���Ƶ����ļ���id:�ļ�id:*/
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
				//���Ƶ���ǰ�ļ����£��ļ���ǰ����bak-
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

			//�ļ�����ʹ����+1
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "update file set file_usenum=file_usenum+1 where file_name='%s'", md5);
			myquery(mysql, msg);
			writelog(INFO, "copy success");
			//write(client_sockfd,COPYSUCCESS,strlen(COPYSUCCESS));//��client�˷��͸��Ƴɹ�
		}

		/*----4��ɾ���ļ�----*/
		/*buf�ṹ->DELETEFILE:�ļ�id:*/
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

			//�ļ�����ʹ����-1
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "update file set file_usenum=file_usenum-1 where file_name='%s'", md5);
			myquery(mysql, msg);
			writelog(INFO, "delete success");
			//write(client_sockfd,DELETESUCCESS,strlen(DELETESUCCESS));//��client�˷���ɾ���ɹ�
		}

		/*------5��������------*/
		/*buf�ṹ->RENAMEFILE:�ļ�id:�ĺ�����:*/
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
			//write(client_sockfd,DELETESUCCESS,strlen(RENAMESUCCESS));//��client�˷���ɾ���ɹ�
		}

	}




	//------------------------------------------�ƺ�����------------------------------------------
	mysql_close(mysql);
	close(client_sockfd);
	close(server_sockfd);
	return 0;
}