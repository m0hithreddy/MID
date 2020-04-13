/*
 * MID_interfaces.c
 *
 *  Created on: 10-Mar-2020
 *      Author: Base code is from the man page of the getifaddrs() function (man 3 getifaddrs)
 */

#include"MID_interfaces.h"
#include"MID_structures.h"
#include"MID_functions.h"
#include<string.h>

#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>

struct network_interface** get_net_if_info(char** include_ifs,long include_ifs_count,char** exclude_ifs,long exclude_ifs_count)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char netmask[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		return NULL;
	}

	struct data_bag* bag=create_data_bag();

	/* Walk through linked list, maintaining head pointer so we
		  can free list later */

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{

		if (ifa->ifa_addr == NULL)
			continue;

		if(include_ifs_count>0 && include_ifs!=NULL)
		{
			int i=0;

			for( ;i<include_ifs_count && include_ifs[i]!=NULL ;i++)
			{
				if(!strcmp(include_ifs[i],ifa->ifa_name))
					break;
			}

			if(i>=include_ifs_count)
				continue;
		}

		if(exclude_ifs_count>0 && exclude_ifs!=NULL)
		{
			int i=0;

			for( ;i<exclude_ifs_count && exclude_ifs[i]!=NULL ;i++)
			{
				if(!strcmp(exclude_ifs[i],ifa->ifa_name))
					break;
			}

			if(i<exclude_ifs_count)
				continue;
		}

		family = ifa->ifa_addr->sa_family;

		/* Display interface name and family (including symbolic
			  form of the latter for the common families) */

		/*printf("%-8s %s (%d)\n",
				ifa->ifa_name,
				(family == AF_PACKET) ? "AF_PACKET" :
						(family == AF_INET) ? "AF_INET" :
								(family == AF_INET6) ? "AF_INET6" : "???",
										family);*/

		/* For an AF_INET* interface address, display the address */

		if (family == AF_INET || family == AF_INET6)
		{

			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
							sizeof(struct sockaddr_in6),
							host, NI_MAXHOST,
							NULL, 0, NI_NUMERICHOST);
			if (s != 0)
				continue;

			s = getnameinfo(ifa->ifa_netmask,
								(family == AF_INET) ? sizeof(struct sockaddr_in) :
										sizeof(struct sockaddr_in6),
										netmask, NI_MAXHOST,
										NULL, 0, NI_NUMERICHOST);

			if (s != 0)
				continue;

			struct network_data* n_data=(struct network_data* )malloc(sizeof(struct network_data));

			//Pushing the if_name

			n_data->data=ifa->ifa_name;
			n_data->len=strlen(ifa->ifa_name)+1;

			place_data(bag,n_data);

			//Pushing the address

			n_data->data=host;
			n_data->len=strlen(host)+1;

			place_data(bag,n_data);

			//Pushing the netmask

			n_data->data=netmask;
			n_data->len=strlen(netmask)+1;

			place_data(bag,n_data);

			//Pushing the family

			n_data->data=(char*)&ifa->ifa_addr->sa_family;
			n_data->len=sizeof(ifa->ifa_addr->sa_family);

			place_data(bag,n_data);

			//printf("\t\taddress: <%s>\n", host);

		}
	}

	freeifaddrs(ifaddr);

	//Retrieving the data from the bag and creating network_interface structures

	struct network_interface** net_if=(struct network_interface**)malloc(sizeof(struct network_interface*)*(1+(bag->n_pockets)/4));

	struct data_pocket* pocket=bag->first;

	long counter=0;

	while(pocket!=NULL)
	{
		net_if[counter]=(struct network_interface*)malloc(sizeof(struct network_interface));

		//Popping the if_name

		net_if[counter]->name=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;

		if(pocket==NULL)
		{
			net_if[counter]=NULL;
			return net_if;
		}

		//Popping the address

		net_if[counter]->address=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;

		if(pocket==NULL)
		{
			net_if[counter]=NULL;
			return net_if;
		}

		//Popping the netmask

		net_if[counter]->netmask=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;

		if(pocket==NULL)
		{
			net_if[counter]=NULL;
			return net_if;
		}

		//Popping the family name

		memcpy(&net_if[counter]->family,pocket->data,pocket->len);

		pocket=pocket->next;

		counter++;
	}

	net_if[counter]=NULL;

	return net_if;

}
