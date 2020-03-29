/*
 * MID_structures.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_STRUCTURES_H_
#define MID_STRUCTURES_H_

#define MAX_DATA_POCKET_SIZE 65536

#define INSERT_BEFORE 00
#define INSERT_AFTER 01
#define INSERT_AT 10

#define DELETE_BEFORE 00
#define DELETE_AFTER 01
#define DELETE_AT 10

struct network_data
{
	void *data;
	long len;
};

struct data_pocket
{
	struct data_pocket *previous;
	void *data;
	struct data_pocket *next;
	long len;
};

struct data_bag{
	struct data_pocket *first;
	struct data_pocket *end;
	long n_pockets;
};

struct data_bag* create_data_bag();

void clear_data_bag(struct data_bag* bag);

void append_data_pocket(struct data_bag* bag,long size);

int insert_data_pocket(struct data_bag* bag,struct data_pocket* refer,struct data_pocket* update,int flag);

int delete_data_pocket(struct data_bag* bag,struct data_pocket* refer,int flag);

int place_data(struct data_bag *bag,struct network_data *n_data);

struct network_data* flatten_data_bag(struct data_bag *bag);


#endif /* MID_STRUCTURES_H_ */

