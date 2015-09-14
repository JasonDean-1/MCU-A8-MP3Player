/*
 *     SD��mp3���������Ƴ���
 *	   ���ܣ�
 			 k1:���š���ͣ
 			 k2:ֹͣ����
 			 k3:��һ��
 			 k4:��һ��
 *     ���ӣ������Զ�ѭ������SD��/sdcard/musicsĿ¼��mp3����
 *
 * Author: Charles Yang (QQ: 582913383)
 * Company: �ﲩ��Ƕ��ʽ ������ʿ���
 * Web: http://www.broadon.cn  http://armdspfpga.taobao.com
 
 ****************************************************************************************
ARM/DSP/FPGAȫϵ�п�����ȫ������ﲩ��Ƕ��ʽ ������ʿ���

����������ҵ�Ƽ����޹�˾
��ַ�������к�����֪��·118�ţ�֪�����ӳ�3��1325��
�绰��13581501210/010-62659896 
���棺010-62659896
��ַ��http://www.broadon.cn
���꣺http://armdspfpga.taobao.com
Q Q�� 313638714 
MSN�� jin--yong@hotmail.com 
Email�� lyw@broadon.cn 
��ϵ�ˣ� ������
******************************************************************************************
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/*�����ڴ�������*/
#define PERM S_IRUSR|S_IWUSR													

/*˫��ѭ���б���Ÿ�����*/
struct song				
{
	char songname[60];
	struct song *prev;
	struct song *next;
};

/*���ӽ���id��*/
pid_t gradchild;

/*�ӽ���id��*/
pid_t pid;

/*�����ڴ��������*/
int shmid;
char *p_addr;
/*�����ڴ����ݸ�ʽ*/
/*|gradchild(���ӽ���PID) |+ |��һ���ֽ�|+ currentsong(��ǰ�����б�Ľڵ�ָ��)|*/

/*���ű��*/
int first_key=1;
int play_flag=0;

/*************************************************
Function name: play
Parameter    : struct song *
Description	 : ���ź���
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void play(struct song *currentsong)
{
	pid_t fd;
	char *c_addr;
	char *p;
	int len;
	char my_song[100]="/sdcard/musics/";
	while(currentsong)
	{
		/*�����ӽ��̣������ӽ���*/
		fd = fork();
		if(fd == -1)
		{	
			perror("fork");
			exit(1);
		}
		else if(fd == 0) //���ӽ���
		{
			/*�Ѹ��������ϸ�·��*/
			strcat(my_song,currentsong->songname);
			p = my_song;
			len = strlen(p);

			/*ȥ���ļ�������'\n'*/
			my_song[len-1]='\0';

			printf("THIS SONG IS %s\n",my_song);
                      /*����madplay������������MP3*/
			execl("/bin/madplay","madplay",my_song,NULL);//���ŵ�ǰ�б�MP3����
			printf("\n\n\n");
		}
		else //�ӽ���
		{
			/*�ڴ�ӳ��*/
			c_addr = shmat(shmid,0,0);

			/*�����ӽ��̵�id�͵�ǰ���Ÿ����Ľڵ�ָ�봫�빲���ڴ�*/
			memcpy(c_addr,&fd,sizeof(pid_t));
			memcpy(c_addr + sizeof(pid_t)+1,&currentsong,4);
			/*ʹ��wait�����ӽ��̣�ֱ�����ӽ��̲�������ܱ����ѣ�
			  ��������ʱ����ʾ����MP3�ڼ�û�а������£������˳�򲥷���һ��MP3*/
			if(fd == wait(NULL))
			{
				currentsong = currentsong->next;
				printf("THE NEXT SONG IS %s\n",currentsong->songname);
			}
		}
	}
}

/*************************************************
Function name: creat_song_list
Parameter    : void
Description	 : ������������˫��ѭ������
Return		 : struct song *
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
struct song *creat_song_list(void)
{	
	FILE *fd;
	size_t size;
	size_t len;
	char *line = NULL;
	struct song *head;
	struct song *p1;
	struct song *p2;
	system("ls /sdcard/musics > song_list");
	fd = fopen("song_list","r");

	p1 = (struct song *)malloc(sizeof(struct song));

	printf("+-------------------- song list --------------------+\n");
	system("ls /sdcard/musics");	
	printf("\n");
	printf("+---------------------------------------------------+\n");
	size = getline(&line,&len,fd);

	strncpy(p1->songname,line,strlen(line));
	head = p1;
	while((size = getline(&line,&len,fd)) != -1) //���ļ��ж�ȡһ�У�ֱ��������ߵ��ļ�βEOF����-1
	{	
		p2 = p1;
		p1 = (struct song *)malloc(sizeof(struct song));
		strncpy(p1->songname,line,strlen(line));
		p2->next = p1;
		p1->prev = p2;	
	}
	p1->next = head;
	head->prev = p1;
	p1 = NULL;
	p2 = NULL;
	system("rm -rf song_list");
	return head;
}
/*************************************************
Function name: startplay
Parameter    : pid_t *��struct song *
Description	 : ��ʼ���ź���
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void startplay(pid_t *childpid,struct song *my_song)
{
	pid_t pid;
	int ret;
	/*�����ӽ���*/
	pid = fork();

	if(pid > 0) //������
	{
		*childpid = pid; //�ӽ���PID��ʼ��
		play_flag = 1;
		sleep(1);
		/*��ȡ�����ڴ汣���pid,��ʼ�����ӽ��̵�pid*/
		memcpy(&gradchild,p_addr,sizeof(pid_t));
	}
	else if(0 == pid) //�ӽ���
	{	
		/*�ӽ��̲���MP3����*/
		play(my_song);
	}
}
/*************************************************
Function name: my_pause
Parameter    : pid_t
Description	 : ��ͣ����
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void my_pause(pid_t pid)
{
	printf("+-------------------- PAUSED! PRESS K1 TO CONTINUE --------------------+\n");
	kill(pid,SIGSTOP); //�����ӽ��̷���SIGSTOP�ź�
	play_flag = 0;
}

/*************************************************
Function name: my_pause
Parameter    : pid_t
Description	 : ֹͣ���ź���
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void my_stop(pid_t g_pid)
{

	printf("+-------------------- STOPPED! PRESS K1 TO START PLAY --------------------+\n");
	kill(g_pid,SIGKILL); //�����ӽ��̷���SIGKILL�ź�
	kill(pid,SIGKILL);   //���ӽ��̷���SIGKILL�ź�
	first_key=1;

}

/*************************************************
Function name: conti_play
Parameter    : pid_t
Description	 : ��������
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void conti_play(pid_t pid)
{
	printf("+-------------------- CONTINUE --------------------+\n");
	kill(pid,SIGCONT); //�����ӽ��̷���SIGCONT�ź�
	play_flag=1;
}

/*************************************************
Function name: next
Parameter    : pid_t
Description	 : ��һ�׺���
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void next(pid_t next_pid)
{
	struct song *nextsong;

	printf("+-------------------- NEXT MP3 --------------------+\n");
	/*�ӹ����ڴ������ӽ��̲��Ÿ����Ľڵ�ָ��*/
	memcpy(&nextsong,p_addr + sizeof(pid_t)+1,4);
	/*ָ�����׸����Ľڵ�*/
	nextsong = nextsong->next;
	/*ɱ����ǰ�������ŵ��ӽ��̣����ӽ���*/
	kill(pid,SIGKILL);
	kill(next_pid,SIGKILL);
	wait(NULL);
	startplay(&pid,nextsong);
}

/*************************************************
Function name: prev
Parameter    : pid_t
Description	 : ��һ�׺���
Return		 : void
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
void prev(pid_t prev_pid)
{
	struct song *prevsong;
	/*�ӹ����ڴ������ӽ��̲��Ÿ����Ľڵ�ָ��*/
	printf("+-------------------- PRIOR MP3 --------------------+\n");
	memcpy(&prevsong,p_addr + sizeof(pid_t)+1,4);
	/*ָ�����׸����Ľڵ�*/
	prevsong = prevsong->prev;
	/*ɱ����ǰ�������ŵ��ӽ��̣����ӽ���*/
	kill(pid,SIGKILL);
	kill(prev_pid,SIGKILL);
	wait(NULL);
	startplay(&pid,prevsong);
}

/*************************************************
Function name: main
Parameter    : void
Description	 : ������
Return		 : int
Argument     : void
Autor & date : Hanson  11,04, 05
**************************************************/
int main(void)
{
	int buttons_fd;
	int key_value;
	struct song *head;

    /*���豸�ļ�*/
	buttons_fd = open("/dev/buttons_broadon", 0);
	if (buttons_fd < 0) {
		perror("open device buttons");
		exit(1);
	}

    printf("\n");
    printf("+-----------------------------------------------------------------------+\n");
	printf("+             ����mini2440/micro2440�������SD��MP3������ϵͳ           +\n");
	printf("+                    ���ʵ�̼�: http://www.broadon.cn                  +\n");
	printf("+             ��������Ƕ��ʽ:  http://armdspfpga.taobao.com/            +\n");
    printf("+-----------------------------------------------------------------------+\n");
    printf("\n");
	
    /*���������б�*/
	head = creat_song_list();
	
    printf("+-----------------------------------------------------------------------+\n");
	printf("+                            FUNCTIONALITY                              +\n");
    printf("+-----------------------------------------------------------------------+\n");
	printf("+           K1:START/PAUSE     K2:STOP   K3:NEXT      K4:PRIOR          +\n");
    printf("+-----------------------------------------------------------------------+\n");
    printf("\n");


  /*�����ڴ棺���ڴ���ӽ���ID�������б�λ��*/
	if((shmid = shmget(IPC_PRIVATE,5,PERM))== -1)
		exit(1);
	p_addr = shmat(shmid,0,0);
	memset(p_addr,'\0',1024);
	
	
	while(1) 
	{
		fd_set rds;
		int ret;

		FD_ZERO(&rds);
		FD_SET(buttons_fd, &rds);

		/*������ȡ��ֵ*/
		ret = select(buttons_fd + 1, &rds, NULL, NULL, NULL);
		if (ret < 0) 
		{
			perror("select");
			exit(1);
		}
		if (ret == 0) 
			printf("Timeout.\n");
		else if (FD_ISSET(buttons_fd, &rds))
		{
			int ret = read(buttons_fd, &key_value, sizeof key_value);
			if (ret != sizeof key_value) 
			{
				if (errno != EAGAIN)
					perror("read buttons\n");
				continue;
			} 
			else
			{
				printf("buttons_value: %d\n", key_value+1);
				
				/*�״β��ţ������ǰ���1*/
				if(first_key){
					switch(key_value)
					{	
					case 0:
						startplay(&pid,head);
						first_key=0;
						break;
					case 1:
					case 2:
					case 3:
						printf("+-------------------- PRESS K1 TO START PLAY --------------------+\n");
						break;
				    default:
						printf("+-------------------- PRESS K1 TO START PLAY --------------------+\n");
						break;
					} //end switch
				}//end if(first_key)
				/*�������״β��ţ�����ݲ�ͬ��ֵ����*/
				else if(!first_key){
				    switch(key_value)
					{
					case 0:
						//printf("play_flag:%d\n",play_flag);
						if(play_flag)
							my_pause(gradchild);
						else
							conti_play(gradchild);
						break;
					case 1:
						my_stop(gradchild);
						break;
					case 2:
						next(gradchild);
						break;
					case 3:
						prev(gradchild);
						break;
					} //end switch
			 }//end if(!first_key)

			}
				
		}
	}

	close(buttons_fd);
	return 0;
}

