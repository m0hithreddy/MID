/*
 * MID_interfaces.h
 *
 *  Created on: 10-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_INTERFACES_H_
#define MID_INTERFACES_H_

#include<bits/sockaddr.h>

struct network_interface
{
	char* name; /* interface name */
	char* address; /* interface address (can have multiple, depends on family) */
	char* netmask; /* subnet mask */
	sa_family_t family;    /* address family (e.g., AF_INET, AF_INET6) */
};

struct network_interface** get_net_if_info(char** include_ifs,long include_ifs_count,char** exclude_ifs,long exclude_ifs_count); /* struct network_interfaces[end]==NULL */

#endif /* MID_INTERFACES_H_ */
