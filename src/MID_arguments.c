/*
 * MID_arguments.c
 *
 *  Created on: 20-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_arguments.h"
#include"MID_functions.h"
#include"MID_structures.h"
#include"MID_socket.h"
#include"MID_unit.h"
#include"MID_state.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

struct data_bag* hdr_bag;

void fill_mid_args(char* key,char* value,struct mid_args* args,int conf_flag)
{
	if(key==NULL)
		return;

	if(value==NULL)
	{
		struct network_data* op_data=(struct network_data*)malloc(sizeof(struct network_data));
		struct data_bag* op_bag=create_data_bag();

		if(conf_flag>=1)
		{
			op_data->data="MID: Configuration file not understood, \"";
		}
		else
		{
			op_data->data="MID: \"";
		}

		op_data->len=strlen(op_data->data);
		place_data(op_bag,op_data);

		op_data->data=key;
		op_data->len=strlen(op_data->data);
		place_data(op_bag,op_data);

		op_data->data="\" option used but value not specified";
		op_data->len=strlen(op_data->data)+1;
		place_data(op_bag,op_data);

		mid_help(flatten_data_bag(op_bag)->data);

	}

	if(!strcmp(key,"output-file"))
	{
		args->output_file=strdup(value);
	}

	else if(!strcmp(key,"interfaces") || !strcmp(key,"exclude-interfaces"))
	{
		int in_flag=0;

		if(!strcmp(key,"interfaces"))
		{
			in_flag=1;
		}


		char*** ifs;
		long* ifs_count;

		if(in_flag)
		{
			ifs=&args->include_ifs;
			ifs_count=&args->include_ifs_count;
		}
		else
		{
			ifs=&args->exclude_ifs;
			ifs_count=&args->exclude_ifs_count;
		}

		struct network_data* n_data=(struct network_data*)malloc(sizeof(struct network_data));

		n_data->data=value;
		n_data->len=strlen(value);

		struct data_bag* if_bag=create_data_bag();
		struct network_data* if_data=(struct network_data*)malloc(sizeof(struct network_data));

		while(1)
		{

			n_data=sseek(n_data,",");

			if(n_data==NULL || n_data->data==NULL || n_data->len==0)
			{
				break;
			}

			n_data=scopy(n_data,",",(char**)(&if_data->data),-1);
			if_data->len=strlen(if_data->data)+1;

			place_data(if_bag,if_data);

			if(n_data==NULL || n_data->data==NULL || n_data->len==0)
			{
				break;
			}
		}

		if(if_bag->n_pockets==0)
		{
			*ifs=NULL;
			*ifs_count=0;
			return;
		}

		*ifs=(char**)malloc(sizeof(char*)*if_bag->n_pockets);
		*ifs_count=if_bag->n_pockets;

		struct data_pocket* pocket=if_bag->first;
		long if_count=0;

		while(pocket!=NULL)
		{
			(*ifs)[if_count]=(char*)malloc(sizeof(char)*pocket->len);
			memcpy((*ifs)[if_count],pocket->data,pocket->len);
			pocket=pocket->next;
			if_count++;
		}

	}

	else if(!strcmp(key,"url"))
	{
		args->url=strdup(value);
	}

	else if(!strcmp(key,"unprocessed-file"))
	{
		args->up_file=strdup(value);
	}


	else if(!strcmp(key,"max-parallel-downloads"))
	{
		args->max_parallel_downloads=atol(value);
	}

	else if(!strcmp(key,"max-unit-retries"))
	{
		args->max_unit_retries=atol(value);
	}

	else if(!strcmp(key,"max-redirects"))
	{
		args->max_redirects=atol(value);
	}

	else if(!strcmp(key,"max-tcp-syn-retransmits"))
	{
		args->max_tcp_syn_retransmits=atol(value);
	}

	else if(!strcmp(key,"max-mirrors"))
	{
		args->max_mirrors=atol(value);

		if(args->max_mirrors<=0)
			args->max_mirrors=LONG_MAX;
	}

	else if(!strcmp(key,"unit-sleep-time"))
	{
		args->unit_retry_sleep_time=atol(value);
	}

	else if(!strcmp(key,"progress-update-time"))
	{
		args->progress_update_time=atol(value);
	}

	else if(!strcmp(key,"entry-number"))
	{
		args->entry_number=atol(value);

		if(args->entry_number<0)
			args->entry_number=0;
	}

	else if(!strcmp(key,"ms-file"))
	{
		args->ms_file=strdup(value);
	}

	else if(!strcmp(key,"print-ms"))
	{
		args->read_ms=1;
		args->ms_file=strdup(value);

	}

	else if(!strcmp(key,"delete-ms"))
	{
		args->clear_ms=1;
		args->ms_file=strdup(value);

	}

	else if(!strcmp(key,"header"))
	{

		struct network_data* n_data=(struct network_data*)malloc(sizeof(struct network_data));
		struct network_data* hdr_data=(struct network_data*)malloc(sizeof(struct network_data));

		hdr_data->data=value;
		hdr_data->len=strlen(value)+1;

		hdr_data=scopy(hdr_data,"= ",(char**)(&n_data->data),-1);

		if(hdr_data==NULL || hdr_data->data==NULL || hdr_data->len==0 || hdr_data->len==1)
		{
			if(conf_flag==0)
				mid_help("MID: Configuration file not understood, header format not correct");
			else
				mid_help("MID: Header format not correct");
		}


		hdr_data=sseek(hdr_data,"= ");

		if(hdr_data==NULL || hdr_data->data==NULL || hdr_data->len==0)
		{
			if(conf_flag==0)
				mid_help("MID: Configuration file not understood, header format not correct");
			else
				mid_help("MID: Header format not correct");
		}

		n_data->len=strlen(n_data->data)+1;

		place_data(hdr_bag,n_data);
		place_data(hdr_bag,hdr_data);
	}

	else if(!strcmp(key,"detailed-progress"))
	{
		if(atol(value)<=0)
		{
			args->detailed_progress=0;
		}
		else
		{
			args->detailed_progress=1;
		}
	}

	else if(!strcmp(key,"force-resume"))
	{
		if(atol(value)<=0)
		{
			args->force_resume=0;
		}
		else
		{
			args->force_resume=1;
		}
	}

	else if(!strcmp(key,"no-resume"))
	{
		if(atol(value)<=0)
		{
			args->no_resume=0;
		}
		else
		{
			args->no_resume=1;
		}
	}
#ifdef LIBSSL_SANE
	else if(!strcmp(key,"detailed-save"))
	{
		if(atol(value)<=0)
		{
			args->detailed_save=0;
		}
		else
		{
			args->detailed_save=1;
		}
	}
#endif
	else if(!strcmp(key,"quiet"))
	{
		if(atol(value)<=0)
		{
			args->quiet_flag=0;
		}
		else
		{
			args->quiet_flag=1;
		}
	}

	else if(!strcmp(key,"verbose"))
	{
		if(atol(value)<=0)
		{
			args->verbose_flag=0;
		}
		else
		{
			args->verbose_flag=1;
		}
	}

	else if(!strcmp(key,"vverbose"))
	{
		if(atol(value)<=0)
		{
			args->vverbose_flag=0;
		}
		else
		{
			args->vverbose_flag=1;
			args->verbose_flag=1;
		}
	}

	else if(!strcmp(key,"surpass-root-check"))
	{
		if(atol(value)<=0)
		{
			args->surpass_root_check=0;
		}
		else
		{
			args->surpass_root_check=1;
		}
	}

	else // If option not valid
	{
		struct network_data* op_data=(struct network_data*)malloc(sizeof(struct network_data));
		struct data_bag* op_bag=create_data_bag();

		if(conf_flag>=1)
		{
			op_data->data="MID: Configuration file not understood, option \"";
		}
		else
		{
			op_data->data="MID: Option \"";
		}

		op_data->len=strlen(op_data->data);
		place_data(op_bag,op_data);

		op_data->data=key;
		op_data->len=strlen(op_data->data);
		place_data(op_bag,op_data);

		op_data->data="\" not known";
		op_data->len=strlen(op_data->data)+1;
		place_data(op_bag,op_data);

		mid_help(flatten_data_bag(op_bag)->data);
	}
}

void read_conf(char* conf,struct mid_args* args)
{
	args->max_unit_retries=MAX_UNIT_RETRIES;
	args->max_redirects=DEFAULT_MAX_HTTP_REDIRECTS;
	args->max_parallel_downloads=MAX_PARALLEL_DOWNLOADS;
	args->max_tcp_syn_retransmits=MAX_TCP_SYN_RETRANSMITS;
	args->max_mirrors=DEFAULT_MAX_MIRRORS;
	args->unit_retry_sleep_time=UNIT_RETRY_SLEEP_TIME;
	args->progress_update_time=PROGRESS_UPDATE_TIME;

	if(conf==NULL)
		return;

	FILE* fp=fopen(conf,"r");

	if(fp==NULL)
	{
		mid_help("MID: Error reading configuration file");
	}

	char* buf[MAX_TRANSACTION_SIZE];
	struct data_bag* conf_bag=create_data_bag();
	struct network_data* conf_data=(struct network_data*)malloc(sizeof(struct network_data));
	conf_data->data=buf;

	long status=0;

	while(1)
	{
		status=read(fileno(fp),buf,MAX_TRANSACTION_SIZE);

		if(status<0)
		{
			fclose(fp);
			mid_help("MID: Error reading configuration file");
		}
		else if(status==0)
		{
			fclose(fp);
			break;
		}

		conf_data->len=status;
		place_data(conf_bag,conf_data);
	}

	conf_data=flatten_data_bag(conf_bag);

	clear_data_bag(conf_bag);



	char* key_buffer;
	char* value_buffer;
	int key_flag=0;

	while(1)
	{
		conf_data=sseek(conf_data," \n");

		if(conf_data==NULL || conf_data->data==NULL || conf_data->len==0)
		{
			if(key_flag==0)
				break;
			else
				fill_mid_args(key_buffer,NULL,args,1);
		}

		if(((char*)conf_data->data)[0]=='#') // Skip the comments
		{
			conf_data=scopy(conf_data,"\n",NULL,-1);
		}
		else
		{
			if(key_flag==0)
			{
				if(((char*)conf_data->data)[0]=='\'')
				{
					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;

					conf_data=scopy(conf_data,"\'",&key_buffer,-1);

					if(conf_data==NULL || conf_data->data==NULL || conf_data->len==0)
					{
						mid_help("MID: Configuration file not understood, missing \" ' \"");
					}

					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;
				}
				else if(((char*)conf_data->data)[0]=='\"')
				{
					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;

					conf_data=scopy(conf_data,"\"",&key_buffer,-1);

					if(conf_data==NULL || conf_data->data==NULL || conf_data->len==0)
					{
						mid_help("MID: Configuration file not understood, missing \" \" \"");
					}

					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;
				}
				else
				{
					conf_data=scopy(conf_data," \n#",&key_buffer,-1);
				}

				key_flag=1;
			}
			else
			{
				if(((char*)conf_data->data)[0]=='\'')
				{
					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;

					conf_data=scopy(conf_data,"\'",&value_buffer,-1);

					if(conf_data==NULL || conf_data->data==NULL || conf_data->len==0)
					{
						mid_help("MID: Configuration file not understood, missing \" ' \"");
					}

					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;
				}
				else if(((char*)conf_data->data)[0]=='\"')
				{
					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;

					conf_data=scopy(conf_data,"\"",&value_buffer,-1);

					if(conf_data==NULL || conf_data->data==NULL || conf_data->len==0)
					{
						mid_help("MID: Configuration file not understood, missing \" \" \"");
					}

					conf_data->data=conf_data->data+1;
					conf_data->len=conf_data->len-1;
				}
				else
				{
					conf_data=scopy(conf_data," #\n",&value_buffer,-1);
				}

				if(conf_data==NULL)
					continue;

				fill_mid_args(key_buffer,value_buffer,args,1);

				key_flag=0;
			}
		}
	}

}

struct mid_args* parse_mid_args(char** argv,long argc)
{

	struct mid_args* args=(struct mid_args*)calloc(1,sizeof(struct mid_args));

	hdr_bag=create_data_bag();

	// Check if config file option is given --conf || -c

	for(int i=0;i<argc;i++)
	{
		if(!strcmp(argv[i],"--conf") || !strcmp(argv[i],"-c"))
		{
			if(i==argc-1)
			{
				mid_help("MID: --conf option used but the file name is not specified");
			}
			read_conf(argv[i+1],args);
			break;
		}

		if(i==argc-1)
		{
			FILE* fp=fopen(DEFAULT_CONFIG_FILE,"r");

			if(fp!=NULL) // Default config file exits use it.
			{
				fclose(fp);
				read_conf(DEFAULT_CONFIG_FILE,args);
			}
			else // No file exits, call the read_conf with NULL to initialize args with default parameters
			{
				read_conf(NULL,args);
			}
		}
	}

	long counter=0;

	// Command name

	args->cmd=(char*)malloc(sizeof(char)*(strlen(argv[0])+1));
	memcpy(args->cmd,argv[0],strlen(argv[0]+1));
	args->arg_count=argc;

	counter++;

	// Parse the command line arguments

	while(counter<argc)
	{
		if(!strcmp(argv[counter],"-h") || !strcmp(argv[counter],"--help"))
		{
			mid_help(NULL);
		}

		if( !strcmp(argv[counter],"--output-file") || !strcmp(argv[counter],"-o") ) // --ouput-file || -o
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("output-file",value,args,0);

			counter++;
		}

		else if( !strcmp(argv[counter],"--interfaces") || !strcmp(argv[counter],"-i") || !strcmp(argv[counter],"--exclude-interfaces") || !strcmp(argv[counter],"-ni")) // --interface || -i || --exclude-interface || -ni
		{

			char* value=NULL;

			int in_flag=0;

			if(!strcmp(argv[counter],"--interfaces") || !strcmp(argv[counter],"-i"))
			{
				in_flag=1;
			}

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			if(in_flag)
				fill_mid_args("interfaces",value,args,0);
			else
				fill_mid_args("exclude-interfaces",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--url") || !strcmp(argv[counter],"-u") )  // --url || -u
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("url",value,args,0);

			counter++;
		}

		else if( !strcmp(argv[counter],"--unprocessed-file") || !strcmp(argv[counter],"-up") ) // --unprocessed-file || -up
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("unprocessed-file",value,args,0);

			counter++;
		}

		else if( !strcmp(argv[counter],"--max-parallel-downloads") || !strcmp(argv[counter],"-n")) // --max-parallel-downloads || -n
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("max-parallel-downloads",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--max-unit-retries") || !strcmp(argv[counter],"-ur")) // --max-unit-retries || -ur
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("max-unit-retries",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--max-redirects") || !strcmp(argv[counter],"-R")) // --max-redirects || -R
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("max-redirects",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--max-tcp-syn-retransmits") || !strcmp(argv[counter],"-sr")) // --max-tcp-syn-retransmits || -sr
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("max-tcp-syn-retransmits",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--max-mirrors") || !strcmp(argv[counter],"-m")) // --max-mirrors || -m
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("max-mirrors",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--unit-sleep-time") || !strcmp(argv[counter],"-us")) // --unit-sleep-time || -us
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("unit-sleep-time",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--progress-update-time") || !strcmp(argv[counter],"-pu")) // --progress-update-time || -pu
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("progress-update-time",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--version") || !strcmp(argv[counter],"-V"))
		{
			printf("\n");
			printf("%s version %s (%s) ",PACKAGE_NAME,PACKAGE_VERSION,HOST_ARCH);

#ifdef LIBSSL_SANE
			printf("+ssl +crypto");
#else
			printf("-ssl -crypto");
#endif

#ifdef LIBZ_SANE
			printf(" +z");
#else
			printf(" -z");
#endif

			printf("\n\n");
			printf("Project homepage: [ %s ]",PACKAGE_URL);
			printf("\n\n");

			exit(1);
		}

		else if(!strcmp(argv[counter],"--header") || !strcmp(argv[counter],"-H")) // --header || -H
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("header",value,args,0);

			counter++;

		}

		else if(!strcmp(argv[counter],"--detailed-progress") || !strcmp(argv[counter],"-pd")) // --detailed-progress || -pd
		{
			args->detailed_progress=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--force-resume") || !strcmp(argv[counter],"-fr")) // --force-resume || -fr
		{
			args->force_resume=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--no-resume") || !strcmp(argv[counter],"-nr")) // --no-resume || -nr
		{
			args->no_resume=1;

			counter++;
		}
#ifdef LIBSSL_SANE
		else if(!strcmp(argv[counter],"--detailed-save") || !strcmp(argv[counter],"-ds")) // --detailed-save || -ds
		{
			args->detailed_save=1;

			counter++;
		}
#endif
		else if(!strcmp(argv[counter],"--entry-number") || !strcmp(argv[counter],"-e")) // --entry-number || -e
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("entry-number",value,args,0);

			counter++;
		}

		else if( !strcmp(argv[counter],"--ms-file") || !strcmp(argv[counter],"-ms") ) // --ms-file || -ms
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("ms-file",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--print-ms") || !strcmp(argv[counter],"-pm") )  // --read-ms || -pm
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("print-ms",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--delete-ms") || !strcmp(argv[counter],"-dm") )  // --delete-ms || -dm
		{
			char* value=NULL;

			counter++;

			if(counter<argc)
			{
				value=argv[counter];
			}

			fill_mid_args("delete-ms",value,args,0);

			counter++;
		}

		else if(!strcmp(argv[counter],"--quiet") || !strcmp(argv[counter],"-q")) // --quiet || -q
		{
			args->quiet_flag=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--verbose") || !strcmp(argv[counter],"-v")) // --verbose || -v
		{
			args->verbose_flag=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--vverbose") || !strcmp(argv[counter],"-vv")) // --vverbose || -vv
		{
			args->verbose_flag=1;
			args->vverbose_flag=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--surpass-root-check") || !strcmp(argv[counter],"-s")) // --surpass-root-check || -s
		{
			args->surpass_root_check=1;

			counter++;
		}

		else if(!strcmp(argv[counter],"--conf") || !strcmp(argv[counter],"-c")) // config file already handled
		{
			counter++;
			//
			counter++;
		}

		else // else option not known
		{
			fill_mid_args(argv[counter],"",args,0);
		}

	}

	// If read_ms flag is set

	if(args->read_ms)
	{
		read_ms_file(args->ms_file,args->entry_number,MS_PRINT);

		exit(0);
	}

	// If clear_ms flag is set

	if(args->clear_ms)
	{
		clear_ms_entry(args->ms_file,args->entry_number,MS_PRINT);

		exit(0);
	}

	// Check for mandatory URL argument {--url | -u}

	if(args->url==NULL)
	{
		mid_help("MID: URL must be specified using {--url | -u} option");
	}

	// Collect and fill the custom HTTP headers from conf_file and cmd_line

	if(hdr_bag->n_pockets!=0)
	{
		if(hdr_bag->n_pockets%2!=0)
		{
			mid_help("MID: Header format not correct");
		}

		char*** custom_headers=(char***)malloc(sizeof(char**)*(1+hdr_bag->n_pockets/2));

		struct data_pocket* pocket=hdr_bag->first;
		long counter=0;

		while(pocket!=NULL)
		{
			custom_headers[counter]=(char**)malloc(sizeof(char*)*2);

			custom_headers[counter][0]=(char*)malloc(sizeof(char)*pocket->len);
			memcpy(custom_headers[counter][0],pocket->data,pocket->len);

			pocket=pocket->next;

			custom_headers[counter][1]=(char*)malloc(sizeof(char)*pocket->len);
			memcpy(custom_headers[counter][1],pocket->data,pocket->len);

			pocket=pocket->next;
			counter++;
		}

		custom_headers[counter]=NULL;
		args->custom_headers=custom_headers;
	}

	return args;
}

void mid_help(char* err_msg)
{
	if(err_msg!=NULL)
	{
		fprintf(stderr,"\n%s\n\n",err_msg);
		fprintf(stderr,"Try {MID | mid} --help for more information \n\n");
		exit(1);
	}

	fprintf(stderr,"Usage: {MID | mid} --url URL [OPTIONS]\n\n");
	fprintf(stderr,"   --output-file file                     -o file               Use this output file instead of determining from the URL. \n");
	fprintf(stderr,"   --interfaces nic1,nic2...              -i nic1,nic2...       Network-interfaces which are used in the file download. \n");
	fprintf(stderr,"   --exclude-interfaces nic1,nic2...      -ni nic1,nic2...      Network-interfaces which are excluded from the file download. \n");
	fprintf(stderr,"   --help                                 -h                    Print this help message. \n");
	fprintf(stderr,"*  --url URL                              -u URL                URL to be used in the download. \n");
	fprintf(stderr,"   --unprocessed-file file                -up file              Use this .up file instead of determining from the URL. \n");
	fprintf(stderr,"   --max-parallel-downloads x             -n x                  At max x parallel connections are opened. \n");
	fprintf(stderr,"   --max-unit-retries x                   -ur x                 At max x retries are made by a unit to download a chunk. \n");
	fprintf(stderr,"   --max-redirects x                      -R x                  At max x HTTP redirects are followed. \n");
	fprintf(stderr,"   --max-tcp-syn-retransmits x            -sr x                 At max x TCP SYNs are retransmitted. \n");
	fprintf(stderr,"   --max-mirrors x                        -m x                  x > 0 => At max x mirrors are used. x <= 0 => All mirrors are used. \n");
	fprintf(stderr,"   --unit-sleep-time x                    -us x                 Download unit sleeps for x seconds before retrying. \n");
	fprintf(stderr,"   --progress-update-time x               -pu x                 Progress information update interval. \n");
	fprintf(stderr,"   --detailed-progress                    -pd                   Show detailed download progress. \n");
	fprintf(stderr,"   --force-resume                         -fr                   Skip the checks and start the download. \n");
	fprintf(stderr,"   --no-resume                            -nr                   Do not resume the partial downloads. Default action is to resume. \n");
#ifdef LIBSSL_SANE
	fprintf(stderr,"   --detailed-save                        -ds                   Save the hashes of partially downloaded content. Default action is not to save. \n");
#endif
	fprintf(stderr,"   --entry-number x                       -e x                  If multiple partial downloads exists, select the x-th download. \n");
	fprintf(stderr,"   --ms-file file                         -ms file              Use this .ms file instead of determining from the URL. \n");
	fprintf(stderr,"   --print-ms file                        -pm file              Print the MID state information and exit. \n");
	fprintf(stderr,"   --delete-ms file                       -dm file              Delete ms entry, entry number should be specified with option -e. \n");
	fprintf(stderr,"   --version                              -V                    Print the version and exit. \n");
	fprintf(stderr,"   --header h=v                           -H h=v                Add custom headers to HTTP request message (can be used multiple times). \n");
	fprintf(stderr,"   --quiet                                -q                    Silent mode, don't output anything. \n");
	fprintf(stderr,"   --verbose                              -v                    Print the verbose information. \n");
	fprintf(stderr,"   --vverbose                             -vv                   Print the very verbose information. \n");
	fprintf(stderr,"   --surpass-root-check                   -s                    If had the sufficient permissions, use -s to surpass the root-check. \n");
	fprintf(stderr,"   --conf file                            -c file               Specify the conf file. Preference order: cmd_line > conf_file > default_values. \n");
	fprintf(stderr,"\n");
	fprintf(stderr,"   * marked arguments are mandatory \n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Project homepage: [ %s ]",PACKAGE_URL);
	fprintf(stderr,"\n\n");
	exit(1);
}
