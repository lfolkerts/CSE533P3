#include <linux/if_ether.h>

#define MSG_CAT_ODR 0x5aba
#define ODR_RREP 0x00
#define ODR_RREQ 0x01
#define ODR_APP 0x02

#define MAX_SOCKFD 30
#define MAX_PACKET_LEN 1514

typedef struct
{
	struct ethhdr ether;
	unsigned char pkt_type; //For ODR, the is RREP, RREQ, APP messages
	typedef union
	{
		struct
		{
			unsigned char f_flag : 1;
			unsigned char rrep_sent : 1;
			unsigned char unused_flag2 : 1;
			unsigned char unused_flag3 : 1;
			unsigned char unused_flag4 : 1;
			unsigned char unused_flag5 : 1;
			unsigned char unused_flag6 : 1;
			unsigned char unused_flag7 : 1;
		}u;
		unsigned char mflag;
	}flags;

	int src_port;
	int dest_port;
	uint32_t id; //random number to give packet an id
	uint16_t hop_cnt;
	struct sockaddr_un next;
	uint16_t msg_len; //packet length
	
}packet_hdr;
