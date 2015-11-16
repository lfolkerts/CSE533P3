/*************************************************************
* route_table module
* This file contains all the neccessary routing table info
*
* updateTable - updates the routing table based on packet header
* update_table_entry - helper function for UpdateTable
* get_next_hop - returns next step in route
* get_odrindex - returns what index in table the route is stored, or a NULL entry if not found
* get_rreq_info - returns stored information of RREQ packets when RREPs come in
*************************************************************/

static rreq_info* rreq_head, *rreq_tail;
/*************************************************
* This function will update the routing table
*
* packet_hdr* p - packet header which will provide us with information updates
* char* local_dest - will be created and filled with next hop destination - NULL if not present
************************************************/
int updateTable(packet_hdr* p, char* local_dest)
{
	rreq_info* info;
	uint8_t force_flag;
	if(p->flags.u.f_Flag==1){force_flag=1;}
	else{force_flag=0;}

	do{ info = malloc(sizeof(rreq_info));}while(info==NULL);

	local_dest = get_next_hop(p->ether.h_dest, ntohs(p->dest_port));

	/* UPDATE SRC PATH */
	update_table_entry(p->org_src, ntohs(p->srcport), ntohs(p->hop_cnt), p->ether.h_source, force_flag);

	/*UPDATE DEST PATH */
	switch(p->pkt_type)
	{ 
                case ODR_RREP: //route reply
                        if(strncmp(p->ether.h_dest, local_dest, ETH_ALEN)!=0)//reply DID NOT rereach itself
                        {
				info = get_rreq_info(ntohl(p->id)); //look for rreq
				update_table_entry(p->org_dest, p->destport, ntohs(p->hop_cnt) - info->hop_basecnt, info->local_src, force_flag);
				free(info);
                        }
                break;
                case ODR_RREQ: //route request - need to keep track of packet for when RREP comes back - append to buffer head
                        if(strncmp(p->ether.h_dest,dest, ETH_ALEN)!=0)
                        {
				//store info for when we recieve the rrep
                                info->hop_basecnt = ntohs(p->hop_cnt);
				info->rreq_time = time(NULL); //will destroy rreq after ROUTEINFO_TTL seconds
				info->id = ntohl(p->id);
				memcpy(info->local_src, p->ether.h_source, ETH_ALEN); 
				//insert it into linked list
				info->next = rreq_head;
				info->prev = NULL;
				rreq_head = info;
				if(rreq_tail ==NULL){rreq_tail = rreq_head;}
                        }
                break;
                case ODR_APP: //application message - nothing to do already updated path to src
                default:
			break;
			
			
				
	}
}
/***********************************************
* Will update the routing table when appropriate
*
* char* dest - final destination MAC address
* int dest_port - final destination port
* int hop_cnt - number of hops between current node and dest
* char* next_hop - next hop in route
* uint8_t forced - forces overwrite in table
********************************************************/
void update_table_entry(char* dest, int dest_port, int hop_cnt, char* next_hop, uint8_t forced)
{
	odr_table_entry route;
	int route_index;

	route_index = get_index();
	route = ODRTable[route_index];
	
	replace_flag = 0;
	if( //replace
		route->saddr == NULL || //no entry 
		(route->time - time(NULL)) > Staleness || //stale entry
		forced || //forced
		route->hop_cnt >= hop_cnt //better route
	)
	{
		//update route
		route->next_hop = next_hop;
		route->hop_cnt = hop_cnt;
		route->time = time(NULL);
		
		send_queued_packets(route);//does this belong here
	}
}


/************************************************
* Allocates and returns the next hop to the destination node
*
* char* dest - final destiation MAC address
* int dest_port - final destination port
*
* returns pointer to next hop; NULL if not found
***********************************************/
char* get_next_hop(char* dest, int dest_port)
{	
	int route_index;
	odr_table_entry route;
	char* next_hop;
	
	route_index = get_odrindex(dest, dest_port);
	if(route_index<0){ return NULL; } //error
	route = ODRTable[route_index];
	if(route == NULL){ return NULL;} //no route
	
	do{ next_hop = malloc(ETH_ALEN)}while(next_hop==NULL);
	memcpy(next_hop, route->next_hop, ETH_ALEN);
	return next_hop;
}

/************************************************
* gets index in ODRTable for us
*
* char* dest - final destiation MAC address
* int dest_port - final destination port
*
* returns index to dest, or index to NULL if not found
* can return negative if err
***********************************************/
int get_odrindex(char* dest, int port)
{
	uint16_t base_index;
	int i;
	
	//hash function
	base_index = 0;
	for(i = 0; i<ETH_ALEN-1; i+=2)
	{
		base_index ^= dest[i];
		base_index ^= dest[i+1]<<8;
	}
	base_index %= ROUTE_TABLE_SIZE;
	
	i = base_index; 
	while(ODRTable[i] != NULL)
	{
		if(strncmp(dest, ODRTable[i]->dest, ETH_ALEN)==0 && dest->port == port)
		{
			break;
		}
		i++;
		i%=ROUTE_TABLE_SIZE;
		if(i==base_cnt){ return -1; }//no destination or empty index found
	}
	return i;
}

/**************************************************
* Returns stored rreq_info packet with the minumum hops
*
* int id - packet id 
***************************************************/
rreq_info* get_rreq_info(int id)
{
	rreq_info *icursor, *freecursor, *ret;
	
	icursor = rreq_tail
	while(icursor != NULL) 
	{
		if(icursor->time - time(NULL) > RREQ_INFO_TTL) //expired node
		{
			if(icursor->next){icursor->next->prev = icursor->prev;}
			else{rreq->tail = icursor->prev; } //no next == tail of list
			if(icursor->prev){icursor->prev->next = icursor->next;}
			else{ rreq_head = icursor->next;} //no prev == head of list

			freecursor = icursor;
			icursor = icursor-> prev;
			free(freecursor);
			continue;
		}
		else if(id == icursor->id) //id match
		{
			if(ret == NULL) //no answer yet
			{
				//keep in table in case we get another rreq
				ret = icursor;
			}
			else if(icursor->hop_basecnt < ret->hop_basecnt) //take lowest hop_basecnt answer, or may get misleading answer
			{
				//remove ret
				if(ret->next){ret->next->prev = ret->prev;}
	                        else{rreq->tail = ret->prev; } //no next == tail of list
        	                if(ret->prev){ret->prev->next = ret->next;}
                	        else{ rreq_head = ret->next;} //no prev == head of list
				free(ret);

				ret = icursor;
			}
			icursor = icursor->prev;
		}
		else
		{
			icursor = icursor->prev;
		}
	}//end while
	
	return ret;
}
