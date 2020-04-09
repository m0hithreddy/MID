/*
 * MID_macros.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_H_
#define MID_H_
#define MID_DEBUG
#define MID_USER_AGENT "MID/1.0"
#define MID_DEFAULT_HTTP_VERSION "1.1"

#include"MID_arguments.h"
#include"MID_functions.h"
#include<stdio.h>

extern struct mid_args* args;
extern FILE* o_fp;
extern FILE* u_fp;

#endif /* MID_H_ */
