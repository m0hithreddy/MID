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
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>

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
		args->output_file=(char*)malloc(sizeof(char)*(strlen(value)+1));
		memcpy(args->output_file,value,strlen(value)+1);
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
		args->url=(char*)malloc(sizeof(char)*(strlen(value)+1));
		memcpy(args->url,value,strlen(value)+1);
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

	else if(!strcmp(key,"unit-sleep-time"))
	{
		args->unit_retry_sleep_time=atol(value);
	}

	else if(!strcmp(key,"progress-update-time"))
	{
		args->progress_update_time=atol(value);
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

			if(fp!=NULL)
			{
				fclose(fp);
				read_conf(DEFAULT_CONFIG_FILE,args);
			}
			else
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

		else if(!strcmp(argv[counter],"--url")) // --url
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

		else if(!strcmp(argv[counter],"--max-unit-retries") || !strcmp(argv[counter],"-Ur")) // --max-unit-retries || -Ur
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

		else if(!strcmp(argv[counter],"--max-redirects") || !strcmp(argv[counter],"-R")) // --max-redirects || -r
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

		else if(!strcmp(argv[counter],"--max-tcp-syn-retransmits") || !strcmp(argv[counter],"-Sr")) // --max-tcp-syn-retransmits || -Sr
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

		else if(!strcmp(argv[counter],"--unit-sleep-time") || !strcmp(argv[counter],"-Us")) // --unit-sleep-time || -Us
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

		else if(!strcmp(argv[counter],"--progress-update-time") || !strcmp(argv[counter],"-Pu")) // --progress-update-time || -Pu
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
			printf("Project homepage: <%s>",PACKAGE_URL);
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

		else if(!strcmp(argv[counter],"--detailed-progress") || !strcmp(argv[counter],"-Pd")) // --detailed-progress || -Pd
		{
			args->detailed_progress=1;

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

	if(args->url==NULL) // mandatory argument --url
	{
		mid_help("MID: URL must be specified using --url option");
	}

	if(hdr_bag->n_pockets!=0) // collect the custom http headers across the configuration file and command line arguments
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
		fprintf(stderr,"Try MID --help for more information \n\n");
		exit(1);
	}

	fprintf(stderr,"Usage: MID --url URL [options]\n\n");
	fprintf(stderr,"  --output-file file                   -o file             Specify the output file, if not specified MID will automatically detect from the URL. \n");
	fprintf(stderr,"  --interfaces nic1,nic2...            -i nic1,nic2...     Network-interfaces which are used in the file download. \n");
	fprintf(stderr,"  --exclude-interfaces nic1,nic2...    -ni nic1,nic2...    Network-interfaces which are excluded from the file download. \n");
	fprintf(stderr,"  --help                               -h                  Print this help message. \n");
	fprintf(stderr,"* --url URL                            nil                 URL to be downloaded. \n");
	fprintf(stderr,"  --max-parallel-downloads x           -n x                Maximum x number of parallel connections are allowed. \n");
	fprintf(stderr,"  --max-unit-retries x                 -Ur x               Maximum x number of retries are made by a unit to download a chunk. If failed, the download is terminated. \n");
	fprintf(stderr,"  --max-redirects x                    -R x                Maximum x number of HTTP redirects are followed. \n");
	fprintf(stderr,"  --max-tcp-syn-retransmits x          -Sr x               At max x TCP SYN are retransmitted when initiating a TCP connection. \n");
	fprintf(stderr,"  --unit-sleep-time x                  -Us x               Download unit sleeps for x seconds before retrying. \n");
	fprintf(stderr,"  --progress-update-time x             -Pu x               Information related to download progress updates after every x seconds. \n");
	fprintf(stderr,"  --detailed-progress                  -Pd                 Show detailed download progress. \n");
	fprintf(stderr,"  --version                            -V                  Print the version and exit. \n");
	fprintf(stderr,"  --header h=v                         -H h=v              Add custom headers to HTTP request message (option can be used multiple times to add multiple headers). \n");
	fprintf(stderr,"  --quiet                              -q                  Silent mode, don't output anything. \n");
	fprintf(stderr,"  --verbose                            -v                  Print the verbose information. \n");
	fprintf(stderr,"  --vverbose                           -vv                 Print the very verbose information. \n");
	fprintf(stderr,"  --surpass-root-check                 -s                  If had the sufficient permissions, use -s to surpass the root-check. \n");
	fprintf(stderr,"  --conf file                          -c file             Specify the configuration file path. Preference order: cmd_line > conf_file > default_values \n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  * marked arguments are mandatory \n");
	fprintf(stderr,"\n");
	exit(1);
}
