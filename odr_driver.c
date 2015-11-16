/**************************************************************
 * ODR_driver module/application
 *
 * This application reads inputs from a file and sends out their packets in our ODR network
 * It will also process any incoming packets and take appropriate action 
 *
 * main - will handle any updates to the send input file AND recieved messages
 * send_rreq - will send out a rreq packet
 * send_rrep - will send out a rrep packet
 * process_header - looks at header to determine appropriate action; sends our RREQs and RREPs
 *************************************************************/

static void SIGCHILD_HADLER(int signo);

/****************************************
 * ./ODR_driver staleness
 *
 * staleness is in seconds
 *****************************************/
int main()
{
	int odr_sendfd, odr_recvfd;
	struct sockaddr_ll odr_saddr;
	char* packet;
	int intf_ind;
	int staleness;
	int size;
	struct sigaction sact_child;
	struct hwa_info *hwa, *hwa_head;

	assert(argc ==2);
	staleness = atoi(argv[1]);
	assert(staleness>=0);

	srand(time(NULL));

	hwa = hwa_head = get_hw_addrs();
	intf_ind = -1;
	while(hwa!=NULL && intf_ind == -1)
	{
		if(is_valid_hwa(hwa))
		{
			intf_ind = hwa->if_index;
		}
		hwa = hwa->next;
	}
	sact_child.sa_handler = SIGCHILD_HANDLER;
	sact_child.sa_mask = 0;
	sact_child.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCHILD, &act, 0);

	odr_sendfd = socket(AF_PACKET, SOCK_RAW, htons(MSG_CAT_ODR));

	FD_ZERO(&odrset);

	do{ packet = malloc(MAX_PACKET_LEN)}while(packet==NULL);

	odr_saddr.sll_family = AF_PACKET;
	odr_saddr.sll_odr_saddr.ol = htons(MSG_CAT_ODR);
	odr_saddr.sll_ifindex = intf_ind;
	odr_saddr.sll_hatype =  ARPHRD_ETHER;
	//odr_saddr.sll_pkttype = PACKET_OTHERHOST;
	odr_saddr.halen = ETH_ALEN;
	//memcpy(odr_saddr.sll_addr, local_dest, ETH_ALEN);

	while(1)
	{
		FD_SET(odr_sendfd, &odrset);
		FD_SET(odr_recvfd, &odrset);
		select(MAX_FD, &odrset, NULL, NULL, NULL);

		if(FD_ISSET(sfile_fd, &rset))
		{
			//get packet info
			size = 0;
			size += recv_from(sfilefd, packet, sizeof(packet_hdr), 0, &rcv_saddr,sizeof(struct sockaddr_ll) );
			p = packet;
			size += recv_from(sfilefd, &packet[sizeof(packet_hdr)], p.pmsg_len, 0, &rcv_saddr,sizeof(struct sockaddr_ll) );

			p.pkt_type = ODR_APP;
			//p.src_port
			//p.dest_port
			p.id = htonl(rand());
			p.hop_cnt = 0;
			//p.orgsrc
			//p.orgdest 
			//p.pmsg_len 

			local_dest = find_route(p.orgdest);
			if(local_dest == NULL)
			{
				send_rreq(odrfd, *p, odr_saddr);
				add_packet(packet);
			}
			else //ready to send
			{
				p.ether.h_proto = htons(MSG_CAT_ODR);
				memcpy(p.ether.dest_ip, local_dest, ETH_ALEN);
				memcpy(p.ether.src_ip, src_mac, ETH_ALEN);

				odr_saddr.sll_pkttype = PACKET_OTHERHOST;
				memcpy(odr_saddr.sll_addr, local_dest, ETH_ALEN);

				err = sendto(filefd, packet, size, 0, &proto, sizeof(struct proto));
				if(err<0)
				{
					add_packet(packet); //try again in a minute
				}
			}
		}//end ISSET(odr_sendfd)
		if(FD_ISSET(odrfd, &rset))
		{
			while(1)
			{
				recv_from(odrfd, packet, sizeof(packet_hdr), 0, &rcv_saddr,sizeof(struct sockaddr_ll) );
				if(bytes != sizeof(packet_hdr)) { break; }
				p = packet;
				recv_from(odrfd, &packet[sizeof(packet_hdr)], p.pmsg_len, 0, &rcv_saddr,sizeof(struct sockaddr_ll) );
				err = process_header(p, local_dest, odrfd); //does not modify header, but will send rreq, rrep

				if(err == MSG_ERR){continue;}
				else if(err == MSG_IGN){err = 0;}
				else if(err == MSG_FWD && local_dest == NULL){	add_packet(packet); }
				else if(err == MSG_FWD) //we have a destination
				{
					p.ether.h_proto = htons(MSG_CAT_ODR);
					memcpy(p.ether.dest_ip, local_dest, ETH_ALEN);
					memcpy(p.ether.src_ip, src_mac, ETH_ALEN);

					odr_saddr.sll_pkttype = PACKET_OTHERHOST;
					memcpy(odr_saddr.sll_addr, local_dest, ETH_ALEN);

					err = sendto(odrfd, packet, size, 0, &proto, sizeof(struct proto));

					if(err<0)
					{
						add_packet(packet); //try again in a minute
					}
				}


				else if(err != MSG_RCVD) //output message to recieve buffer
				{

				}

			}//end odr_recv while(1)
		}//end if ISSET odr_recv
	}//end while(1) (outer)
}

/*************************************************
* send_rreq
*
* int sockfd - socket fd to send data out of
* packet_hdr p - packet header; will be modified appropriately & sent out
* sockaddr_ll s - sample sockaddr; will be modified appropriate for this communication
*************************************************/
int send_rreq(int sockfd, packet_hdr p, struct sockaddr_ll s)
{
	packet_hdr* p;
	struct odr_saddr;
	int err; 

	//p.ether.h_proto
	memcpy(p.ether.dest_ip, 0, ETH_ALEN);
	//p.ether.src_ip

	p.pkt_type = ODR_RREQ;
	//p.src_port
	//p.dest_port
	//p.id - stays the same to detect duplicates
	p.hop_cnt = htons(ntohs(p.hop_cnt)+1);
	//p.orgsrc
	//p.orgdest 
	p.pmsg_len = 0;

	s.sll_pkttype = PACKET_BROADCAST;
	memcpy(s.sll_addr, 0, ETH_ALEN);

	return sendto(sockfd, p, sizeof(struct packet_hdr), 0, &odr_saddr, sizeof(struct sockaddr_ll));
}

/*************************************************
* send_rrep
*
* int sockfd - socket fd to send data out of
* packet_hdr p - packet header; will be modified appropriately & sent out
* sockaddr_ll s - sample sockaddr; will be modified appropriate for this communication
*************************************************/
int send_rrep(int sockfd, packet_hdr org_p, struct sockaddr_ll s)
{
	packet_hdr* p;
	int err;

	memcpy(p, &org_p, sizeof(packet_hdr)); //copy over information - much is still relevant

	//p.ether.h_proto
	memcpy(p.ether.dest_ip, 0, ETH_ALEN);
	//p.ether.src_ip

	p.pkt_type = ODR_RREP;
	//p.src_port
	//p.dest_port
	//p.id - stays the same to detect base hop cnt, detect dups
	//p.hop_cnt  - total hop count
	//p.orgsrc
	//p.orgdest 
	p.pmsg_len = 0;

	s.sll_pkttype = PACKET_BROADCAST;
	memcpy(s.sll_addr, 0, ETH_ALEN);

	return sendto(sockfd, p, sizeof(struct packet_hdr), 0, &odr_saddr, sizeof(struct sockaddr_ll));
}

/*************************************************
* send_rreq
*
* int sockfd - socketfd to send RREPs and RREQs out of
* packet_hdr* p - packet header to interpret
* sockaddr_ll s - sample sockaddr for sending RREQs and RREPs 
*************************************************/
int process_header(int sockfd, const packet_hdr* p, struct sockaddr_ll s)
{
	int err = 0;
	int var;
	char src[ETH_ALEN], dest[ETH_ALEN]
		if(p->ether.h_proto != MSG_CAT_ODR){return MSG_IGN;} //not our message

	//get info
	update_table(p, src, dest);

	switch(p->pkt_type)
	{
		case ODR_RREP: //route reply
			if(dest == 0) //should be a route back
			{
				send_rreq(sockfd,p,s);
				return MSG_ERR; 
			}
			else if(p->ether.h_dest == dest)//reply rereached itself
			{
				return MSG_IGN;
			}
			else if(p->ether.h_src != src && src != DUP_RREP)
			{
				send_rrep(sockfd ,p,s); //forward rrep
				return MSG_IGN;
			}

			break;
		case ODR_RREQ: //route request
			if(dest == 0) //still no route to dest
			{
				send_rreq(sockfd ,p,s);
				return MSG_IGN;
			}	
			else if(p->ether.h_dest == dest)
			{
				send_rrep(sockfd ,p,s);
				return MSG_IGN;
			}
			else if(dest != DUP_RREQ)
			{
				send_rrep(sockfd ,p,s);
				return MSG_IGN;
			}
			break;
		case ODR_APP: //application message
			if(dest == 0) //no route to dest, issue RREQ
			{
				send_rreq(sockfd ,p,s);
				return MSG_FWD; //packet not for us	
			}
			else if(p->ether.h_dest == dest) //we recived the packet
			{
				return MSG_RCVD; //let the msg recv know we got it
			}
			else //forward messge to dest
			{
				return MSG_FWD; //
			}



			break;
		default:
			return -1; //error
			break;
	}
	return -1;
}
}
