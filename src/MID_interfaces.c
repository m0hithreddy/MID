/*
 * MID_interfaces.c
 *
 *  Created on: 10-Mar-2020
 *      Author: Base code is from the man page of the getifaddrs() function (man 3 getifaddrs)
 */

#include"MID_interfaces.h"
#include"MID_structures.h"
#include"MID_functions.h"
#include"MID.h"
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

struct mid_interface** get_mid_interfaces()
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char netmask[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		return NULL;
	}

	struct mid_bag* bag=create_mid_bag();

	/* Walk through linked list, maintaining head pointer so we
		  can free list later */

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{

		if (ifa->ifa_addr == NULL)
			continue;

		if(args->include_ifs_count>0 && args->include_ifs!=NULL)
		{
			long i=0;

			for( ;i<args->include_ifs_count && args->include_ifs[i]!=NULL ;i++)
			{
				if(!strcmp(args->include_ifs[i],ifa->ifa_name))
					break;
			}

			if(i>=args->include_ifs_count)
				continue;
		}

		if(args->exclude_ifs_count>0 && args->exclude_ifs!=NULL)
		{
			long i=0;

			for( ;i<args->exclude_ifs_count && args->exclude_ifs[i]!=NULL ;i++)
			{
				if(!strcmp(args->exclude_ifs[i],ifa->ifa_name))
					break;
			}

			if(i<args->exclude_ifs_count)
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

			struct mid_data* n_data=(struct mid_data* )malloc(sizeof(struct mid_data));

			//Pushing the if_name

			n_data->data=ifa->ifa_name;
			n_data->len=strlen(ifa->ifa_name)+1;

			place_mid_data(bag,n_data);

			//Pushing the address

			n_data->data=host;
			n_data->len=strlen(host)+1;

			place_mid_data(bag,n_data);

			//Pushing the netmask

			n_data->data=netmask;
			n_data->len=strlen(netmask)+1;

			place_mid_data(bag,n_data);

			//Pushing the family

			n_data->data=(char*)&ifa->ifa_addr->sa_family;
			n_data->len=sizeof(ifa->ifa_addr->sa_family);

			place_mid_data(bag,n_data);

			//printf("\t\taddress: <%s>\n", host);

		}
	}

	freeifaddrs(ifaddr);

	//Retrieving the data from the bag and creating mid_interface structures

	struct mid_interface** net_if=(struct mid_interface**)malloc(sizeof(struct mid_interface*)*(1+(bag->n_pockets)/4));

	struct mid_pocket* pocket=bag->first;

	long counter=0;

	while(pocket!=NULL)
	{
		net_if[counter]=(struct mid_interface*)malloc(sizeof(struct mid_interface));

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

	if(args->include_ifs_count)
		sort(net_if,sizeof(struct mid_interface*),0,counter-1,compare_mid_interfaces);

	return net_if;
}

void* compare_mid_interfaces(void* a,void* b)
{
	int* res=(int*)malloc(sizeof(int));

	for(long i=0;i<args->include_ifs_count;i++) // Should return with in the loop;
	{
		if(!strcmp(args->include_ifs[i],(*(struct mid_interface**)a)->name))
		{
			*res=1;
			return (void*)res;
		}

		if(!strcmp(args->include_ifs[i],(*(struct mid_interface**)b)->name))
		{
			*res=0;
			return (void*)res;
		}
	}

	*res=0;
	return res;
}
