/* NOTE: �𧋦蝔见�誩��靘𥟇炎撽埈聢撘𧶏�䔶蒂�𧊋瑼Ｘ䰻 congestion control ��� go back N */
/* NOTE: UDP socket is connectionless嚗峕�𤩺活�喲��閮𦠜�舫�質�����餜p����惩藁 */
/* HINT: 撱箄降雿輻鍂 sys/socket.h 銝剔� bind, sendto, recvfrom */

/*
 * ���朞�讐�嚗�
 * �𧋦甈∩�𨀣平銋� agent, sender, receiver �賣�蝬�摰�(bind)銝��� UDP socket �䲰���䌊����惩藁嚗𣬚鍂靘��𦻖�𤣰閮𦠜�胯��
 * agent�𦻖�𤣰閮𦠜�舀�嚗䔶誑�䔄靽∠�雿滚�����惩藁靘�颲刻�滩�𦠜�臭��䌊sender���糓receiver嚗���飛�穃���⊿�雿𨀣迨�ế�𪃾嚗��删�箸���㕑�𦠜�臬�摰帋��䌊agent��
 * �䔄��閮𦠜�舀�嚗众ender & receiver 敹�����𣂼��䰻�� agent ��雿滚�����惩藁嚗𣬚�嗅�𣬚鍂���滨�摰帋�� socket �唾�羓策 agent �朖�虾��
 * (憒� agent.c 蝚�126銵�)
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

#define DEFAULT_THREASHOLD 16
#define TIME_ONE_MORE_RECV 0.1
#define TIME_WAIT_RECV 0.3
#define TIME_BETWEEN_SEGMENT 0.1
#define TIME_ACK_TIMEOUT 0.8

#define MAX_WINDOW 5000
#define ALARM(sec) ualarm(sec*1000*1000, 0)
#define MAX(a, b) a > b ? a : b

int timedout = 0;

void handler(int s) {
    timedout = 1;
}

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct {
	header head;
	char data[1000];
} segment;

void getSelfIP(char *ip) {
    struct ifaddrs *ifaddr, *ifa;
    int s;
    if(getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "��㮖�滚��䌊撌梁�ip QQ\n");
        exit(1);
    }
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr != NULL) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip,
                    NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if(strcmp(ifa->ifa_name, "enp3s0") == 0
                    && ifa->ifa_addr->sa_family == AF_INET) {
                if(s != 0) {
                    fprintf(stderr, "��㮖�滚��䌊撌梁�ip QQ\n");
                    exit(1);
                } else {
                    return;
                }
            }
        }
    }
    fprintf(stderr, "��㮖�滚��䌊撌梁�ip QQ\n");
    exit(1);
}

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost") == 0) {
        //getSelfIP(dst);
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}

void sendBufferedSegments(int agentsocket, segment *buffer, int buff_len, struct sockaddr_in receiver, float loss_rate) {
    int i;
    for(i = 0; i < buff_len; i++) {
        if (rand() % 100 < 100 * loss_rate) {
            printf("drop data #%d\n", buffer[i].head.seqNumber);
        } else {
            sendto(agentsocket, &buffer[i], sizeof(buffer[i]), 0,
                   (struct sockaddr *)&receiver, sizeof(receiver));
            printf("fwd	data	#%d\n", buffer[i].head.seqNumber);
        }
    }
}

int getFirstNoneAcked(int *mem, int win_size, int win_start_idx) {
    int i;
    for(i = 0; i < win_size; i++) {
        if(mem[i + win_start_idx] == 0) {
            return i + win_start_idx;
        }
    }
    return -1;
}

long getCurTime() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_nsec / 1.0e6 + spec.tv_sec * 1000;
}

int main(int argc, char* argv[]){
    int agentsocket, portNum, nBytes;
    float loss_rate;
    segment s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size;
    char ip[3][50];
    int port[3], i;

    /* FOR Judging */
    int win_size = 1;
    int threshold = DEFAULT_THREASHOLD;
    segment segment_buffer[MAX_WINDOW];
    int win_offset = 0;
    int mem_ack[MAX_WINDOW];
    int win_start_idx = 1;
    int first_none_acked = -1;
    int first = 1;
    long time_buffer_sent = -1;
    int expect_fin = 0, fin_received = 0;
    memset(mem_ack, 0, sizeof(mem_ack));
    /**************/

    if(argc != 7){
        fprintf(stderr,"�鍂瘜�: %s <sender IP> <recv IP> <sender port> <agent port> <recv port> <loss_rate>\n", argv[0]);
        fprintf(stderr, "靘见�: ./agent local local 8887 8888 8889 0.3\n");
        exit(1);
    } else {
        setIP(ip[0], argv[1]);
        setIP(ip[1], "local");
        setIP(ip[2], argv[2]);

        sscanf(argv[3], "%d", &port[0]);
        sscanf(argv[4], "%d", &port[1]);
        sscanf(argv[5], "%d", &port[2]);

        sscanf(argv[6], "%f", &loss_rate);
    }

    /*Create UDP socket*/
    agentsocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in sender struct*/
    sender.sin_family = AF_INET;
    sender.sin_port = htons(port[0]);
    sender.sin_addr.s_addr = inet_addr(ip[0]);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    

    /*bind socket*/
    bind(agentsocket,(struct sockaddr *)&agent,sizeof(agent));

    /*Configure settings in receiver struct*/
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port[2]);
    receiver.sin_addr.s_addr = inet_addr(ip[2]);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 

    /*Initialize size variable to be used later on*/
    sender_size = sizeof(sender);
    recv_size = sizeof(receiver);
    tmp_size = sizeof(tmp_addr);

    printf("�虾隞仿�见�𧢲葫��备Q^\n");
    printf("sender info: ip = %s port = %d and receiver info: ip = %s port = %d\n",ip[0], port[0], ip[2], port[2]);
    printf("agent info: ip = %s port = %d\n", ip[1], port[1]);

    int total_data = 0;
    int drop_data = 0;
    int segment_size, index;
    char ipfrom[1000];
    char *ptr;
    int portfrom;
    srand(time(NULL));
    struct sigaction int_handler = {.sa_handler=handler};
    sigaction(SIGALRM, &int_handler, 0);
    while(1){
        /*Receive message from receiver and sender*/
        memset(&s_tmp, 0, sizeof(s_tmp));
        ALARM(TIME_WAIT_RECV);
        segment_size = recvfrom(agentsocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);

        if(timedout) {
            timedout = 0;
            // NOTE: 瑼Ｘ䰻window�喳�䔶�瘝�
            if(!first && win_offset < win_size) {
                if(fin_received) {
                    continue;
                }
                if (getCurTime() - time_buffer_sent >= (long)(TIME_BETWEEN_SEGMENT * 1000)) {
                    // NOTE: ��身撌脩�栞���璅蹱��偏鈭�嚗�����銝�瘜�
                    printf("憟賢�誩�蝯𣂼偏��轮��见�见�喲�� #%d ~ #%d\n", win_start_idx, win_start_idx + win_offset - 1);
                    sendBufferedSegments(agentsocket, segment_buffer, win_offset, receiver, loss_rate);
                    time_buffer_sent = getCurTime();
                    expect_fin = 1;
                }
            } else if(!first) {
                if(getCurTime() - time_buffer_sent >= (long)(TIME_ACK_TIMEOUT * 1000)) {
                    // NOTE: ack timeout!!
                    first_none_acked = getFirstNoneAcked(mem_ack, win_size, win_start_idx);
                    printf("#%d ack timeout\n", first_none_acked);
                    threshold = MAX(1, win_size / 2);
                    win_offset = 0;
                    win_start_idx = first_none_acked;
                    win_size = 1;
                }
            }
            continue;
        }

        if (segment_size > 0)
        {
            portfrom = ntohs(tmp_addr.sin_port);
            inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));

            if (strcmp(ipfrom, ip[0]) == 0 && portfrom == port[0])
            {
                first = 0;
                /*segment from sender, not ack*/
                if (s_tmp.head.ack)
                {
                    fprintf(stderr, "�𤣰��靘��䌊 sender �� ack segment\n");
                }
                if(win_size == win_offset) {
                    fprintf(stderr, "ERROR: ���銁蝑缆imeout撠勗�單𨭬镼輸�𦒘�\n");
                    exit(1);
                }
                total_data++;
                segment_buffer[win_offset++] = s_tmp;
                if (s_tmp.head.fin == 1)
                {
                    printf("get     fin\n");
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
                    printf("fwd     fin\n");
                    fin_received = 1;
                }
                else
                {
                    if(expect_fin) {
                        fprintf(stderr, "ERROR: window size 憭芸�𧶏�\n");
                        exit(1);
                    }
                    index = s_tmp.head.seqNumber;
                    if(index != win_offset + win_start_idx - 1) {
                        fprintf(stderr, "ERROR: ��摨𤩺�㕑炊嚗���鞱��𤣰��#%d嚗�㭱�𤣰��#%d\n", win_start_idx+win_offset-1, index);
                        exit(1);
                    }
                    if (rand() % 100 < 100 * loss_rate)
                    {
                        drop_data++;
                        // TODO: ��毺���𡃏���嗘����
                        printf("drop	data	#%d,	loss rate = %.4f\n", index, (float)drop_data / total_data);
                    }
                    else
                    {
                        printf("get	data	#%d\n", index);
                    }

                    printf("win_offset = %d, win_size = %d, threshold = %d\n", win_offset, win_size, threshold);
                    if (win_offset == win_size) {
                        // NOTE: 閰西�堒�齿𤣰�见�����讠��
                        ALARM(TIME_ONE_MORE_RECV);
                        segment_size = recvfrom(agentsocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
                        if (segment_size > 0) {
                            // ��㕑���坔�𡁻��鈭�
                            fprintf(stderr, "ERROR: window size 憭芸之嚗�");
                            exit(1);
                        }
                        timedout = 0;
                        printf("��见�见�喲�� #%d ~ #%d\n", win_start_idx, win_start_idx + win_offset - 1);
                        sendBufferedSegments(agentsocket, segment_buffer, win_offset, receiver, loss_rate);
                        time_buffer_sent = getCurTime();
                    }
                }
            }
            else if (strcmp(ipfrom, ip[2]) == 0 && portfrom == port[2])
            {
                /*segment from receiver, ack*/
                if (s_tmp.head.ack == 0)
                {
                    fprintf(stderr, "�𤣰��靘��䌊 receiver �� non-ack segment\n");
                }
                if (s_tmp.head.fin == 1)
                {
                    printf("get     finack\n");
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
                    printf("fwd     finack\n");
                    break;
                }
                else
                {
                    index = s_tmp.head.ackNumber;
                    mem_ack[index] = 1;
                    printf("get     ack	#%d\n", index);
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
                    printf("fwd     ack	#%d\n", index);

                    if(win_offset == win_size) {
                        first_none_acked = getFirstNoneAcked(mem_ack, win_size, win_start_idx);
                        if(first_none_acked == -1) {
                            printf("�𤣰�� #%d ~ #%d ack\n", win_start_idx, win_start_idx + win_size - 1);
                            // NOTE: 憓𧼮�拎indow size
                            win_offset = 0;
                            win_start_idx += win_size;
                            if(win_size < threshold) {
                                win_size *= 2;
                            } else {
                                win_size++;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}