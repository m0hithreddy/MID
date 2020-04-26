/*
 * MID_structures.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_STRUCTURES_H_
#define MID_STRUCTURES_H_

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

struct mid_pocket
{
	struct mid_pocket *previous;
	void *data;
	struct mid_pocket *next;
	long len;
};

struct mid_bag{
	struct mid_pocket *first;
	struct mid_pocket *end;
	long n_pockets;
};

struct mid_bag* create_mid_bag();

void clear_mid_bag(struct mid_bag* bag);

void append_mid_pocket(struct mid_bag* bag,long size);

int insert_mid_pocket(struct mid_bag* bag,struct mid_pocket* refer,struct mid_pocket* update,int flag);

int delete_mid_pocket(struct mid_bag* bag,struct mid_pocket* refer,int flag);

int place_data(struct mid_bag *bag,struct network_data *n_data);

struct network_data* flatten_mid_bag(struct mid_bag *bag);


#endif /* MID_STRUCTURES_H_ */
