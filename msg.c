/*******************************************************************
* msg module
* This file contains the API functions to send and recieve packets
* The packet buffers are formed and written to a file
* The ODR_driver will send out these files for us
*
* OpenSocket - needs to be called before sending to open a the socket
* msg_send - API to send the message
* msg_recv - API to recieve a message
*******************************************************************/
#include "msg.h"
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>


int srcPort[MAX_SOCKFD-3];
struct sockaddr_un srcAddr[MAX_SOCKFD-3];

/*****************************************************
* Creates a temporary file for us to write to
* returns 1 on success, negative number on failure
******************************************************/
int OpenSocket()
{
	int err;
	int sockfd, sunfd;
	struct sockaddr_un* saddr;
	uint16_t srcport;
	err = 0;

	do{saddr = malloc(sizeof(struct sockaddr_un)); }while(saddr==NULL);

	//set up client "address"
	saddr.sun_family = AF_PACKET;
	strncpy(saddr.sun_name, SUN_NAME, 108);
	err = sunfd = mkstemp(saddr.sun_name);
	if(err<0){free(saddr); return err;} 

	//set up socket
	err = sockfd = socket(AF_PACKET, SOCK_RAW, htons(MSG_CAT_ODR));
	if(sockfd<0){ goto os_exit; }
	if(sockfd > MAX_SOCKFD){return -1;}	
	err = bind(sockfd, (struct sockaddr*) saddr, sizeof(struct sockaddr_un));
	if(err<0){goto os_exit;}
	srcport = rand()%(MAX_PORT-MIN_PORT)+MIN_PORT;

	//store information
	srcPort[sockfd-3] = srcport;
	srcAddr[sockfd-3] = saddr;
	err = 1;
os_exit:
	if(err<=0){free(saddr);}
	unlink(saddr.sun_path);
	close(sunfd);
	return err;

}

/***************************************************************
* API function to send a packet
*
* int filefd - giving the socket descriptor for write
* char* dest_ip - giving the ‘canonical’ IP address for the destination node, in presentation format
* int dest_port - gives the destination ‘port’ number
* char* msg - gives the message to be sent
* int flag - if set, it forces a route rediscovery to the destination node even if a non-‘stale’ route already exists
*
* returns TODO
***************************************************************/
int msg_send(int filefd, char* dest_ip, uint16_t dest_port, char* msg, int flag)
{
	int err;
	packet_hdr p;
	int size;	
	char* packet;
	struct sockaddr_ll proto;

	if(srcPort[sockfd-3]<MIN_PORT){return -1;} //not initialized

	proto.sll_family = AF_PACKET;
	proto.sll_protocol = htons(MSG_CAT_ODR);
	proto.sll_ifindex = 2;
	proto.sll_hatype =  ARPHRD_ETHER;
	proto.sll_pkttype = PACKET_OTHERHOST;
	proto.halen = ETH_ALEN;
	memcpy(p.proto.sll_addr, dest_ip, ETH_ALEN);

	//p.ether
	//p.pkt_type
	if(flag!=0) { p.flags.u.f_flag =1;}
	else{p.flags.u.f_flag=0;}
	p.src_port = htons(src_port);
	p.dest_port = htons(dest_port);
	//p.id
	//p.hop_cnt
	memcpy(p.orgsrc, src_mac, ETH_ALEN);
	memcpy(p.orgdest, dest_ip, ETH_ALEN);
	p.pmsg_len = htons(sizeof(msg));

	size  = sizeof(packt_hdr) + sizeof(msg);
	if(size > MAX_PACKET_LEN){return -1;}
	do{ packet = malloc(size+1)}while(packet==NULL);
	memcpy(dest_ip, packet,sizeof(packet_hdr));
	strcpy(msg, &packet[sizeof(packet_hdr)]);

	err = sendto(filefd, packet, size, 0, &proto, sizeof(struct proto));
	if(err <0) return err;
	//else
	return sockfd; 
}
/***************************************************************
* API function to send a packet
*
* int filefd - gives socket descriptor for read
* char* recvd_msg - giving message received
* char* src_ip - giving ‘canonical’ IP address for the source node of message, in presentation format
* int* src_port - gives source ‘port’ numb
*
* returns 1 on success, negative int on error
***************************************************************/

int msgRecv(int filefd, char* recvd_msg, char* src_ip, int* src_port)
{
	int err;
	packet_hdr p;
	struct sockaddr_un srvaddr;
	int size;	
	char* packet;	

	fd_set rcvfds;
	struct timeval tv;

	err = 0;

	if(recvdmsg==NULL){return -1;}
	do{ packet = malloc(MAX_PACKET_LEN)}while(packet==NULL);
	srvaddr.sun_family = AF_LOCAL;
	strncpy(srvaddr.sun_name, SRV_DOMAINNAME, 108);

	//prep select call
	FD_ZERO(&rcvfds);
	FD_SET(sockfd, &rcvfds);
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	err = select(MAX_SOCKFD+1, &rcvfds, NULL, NULL, &tv);
	if(err<0){goto mrcv_exit;}

	if(!FD_ISSET(socketfd, &rcvfds)){err=-1; goto recv_exit;} //timeout

	size = recvfrom(sockfd, packet, 0, MAX_PACKET_LEN, 0, (struct sockaddr*) srvaddr, sizeof(struct sockaddr_un));
	if(size<sizeof(packet_hdr)){return -1;} //should be at least packet_hdr bytes

	memcpy(&p, packet, sizeof(packet_hdr));	
	p.ether.h_proto = ntohs(p.ether.h_proto);
	p.src_port = ntohl(p.src_port);
	p.dest_port = ntohl(p.dest_port);
	p.rand = ntohl(p.rand);
	p.hop_cnt = ntohs(p.hop_cnt);
	p.msg_len = ntohs(p.msg_len);

	err = process_header(&p);

	if(err == MSG_ERR){goto mrcv_exit;}
	else if(err == MSG_IGN){err = 0; goto mrcv_exit;}
	else if(err == MSR_FWD){/*append to circular buffer*/}
	else if(err != MSG_RCVD){err = -1; goto rcv_exit;}
	//else we are good
	size = (p.msg_len<sizeof(recvdmsg))?p.msg_len:sizeof(recvdmsg);
	memcpy(recvdmsg, &packet[sizeof(packet_hdr)], size);
	err = 1;
mrcv_exit:
	free(packet);
	//packet types
	return err;	
}
