
AC_INIT(MID,[1.0])

#safety check in case user overwritten --srcdir
AC_CONFIG_SRCDIR([src/MID.c])

AC_CONFIG_AUX_DIR([build-aux])

#Check for necessary programs
AC_PROG_CC

AC_PROG_SED

AC_PROG_INSTALL

AC_PROG_AWK

AC_PROG_LN_S

AC_PROG_MKDIR_P

AC_CHECK_PROG([TXT2MAN],[txt2man],[yes])

if test "x${TXT2MAN}" != "xyes" ; then
    AC_MSG_NOTICE([txt2man not found, no support for building man pages])
fi

AM_CONDITIONAL([TXT2MAN], [test "x${TXT2MAN}" = "xyes"])

# Some predefined headers

AC_FUNC_MALLOC

#Check for System HEADERS

AC_CHECK_HEADERS( [ \
					stdio.h \
					stdlib.h \
					string.h \
					unistd.h \
					netdb.h \
					netinet/in.h \
					sys/socket.h \
					arpa/inet.h \
					limits.h \
					bits/sockaddr.h \
					arpa/inet.h \
					ifaddrs.h \
					linux/if_link.h \
					errno.h \
					time.h \
					pthread.h \
					ctype.h \
					], [], [AC_MSG_ERROR([$ac_header is required but not found. Headers provided by libc, installed by default in all distributions.])] )

AC_CHECK_FUNCS( [ \
				 printf \
				 strcmp \
				 getuid \
				 geteuid \
				 fprintf \
				 snprintf \
				 sprintf \
				 pwrite \
				 exit \
				 atoi \
				 atol \
				 strtol \
				 malloc \
				 calloc \
				 free \
				 strcpy \
				 strncpy \
				 strcat \
				 strlen \
				 strstr \
				 strcasestr \
				 bzero \
				 fileno \
				 fopen \
				 fclose \
				 time \
				 memcpy \
				 memset \
				 memcmp \
				 sleep \
				 read \
				 write \
				 getifaddrs \
				 getnameinfo \
				 htons \
				 inet_pton \
				 socket \
				 setsockopt \
				 bind \
				 connect \
				 gethostbyname \
				 inet_ntop \
				 isalpha \
				 tolower \
				 ], [], [AC_MSG_ERROR([$ac_func is required but not found. Should be included in libc, check the libc variant installed in your system.])] )
				 
AC_CHECK_LIB(pthread,pthread_create, [], [AC_MSG_ERROR([-lpthread is required but not found. Installed by default in all distributions])])

AC_CHECK_FUNCS( [ \
				   pthread_create \
				   pthread_join \
				   pthread_mutex_init \
				   pthread_mutex_lock \
				   pthread_mutex_unlock \
				   pthread_mutex_destroy \
				   ], [], [AC_MSG_ERROR([$ac_func required but not found. Function provided by libpthread, installed by default in all distributions])] )

AC_CHECK_LIB(crypto,BIO_new, [], [AC_MSG_ERROR([-lcrypto is required but not found. Try installing libssl-dev or similar packages.])] )

AC_CHECK_LIB(ssl,SSL_new, [], [AC_MSG_ERROR([-lssl is required but not found. Try installing libssl-dev or similar packages.])], [-lcrypto] ) 

AC_CHECK_HEADERS([ \
				   openssl/ssl.h \
				   openssl/err.h \
				   openssl/evp.h \
				   openssl/tls1.h\
				   ], [], [AC_MSG_ERROR([$ac_header is required but not found. Header provided by libssl, try installing libssl-dev or similar packages.])] )

AC_CHECK_FUNCS( [ \
				   TLS_method \
				   SSL_CTX_new \
				   SSL_new \
				   SSL_set_fd \
				   SSL_connect \
				   SSL_write \
				   SSL_read \
				   ], [], [AC_MSG_ERROR([$ac_func required but not found. Function provided by libssl, try installing libssl-dev or similar packages.])] )
				   
AC_CHECK_DECLS( [ \
				  SSL_library_init \
				  OpenSSL_add_all_algorithms \
				  SSL_load_error_strings \
				  SSL_set_tlsext_host_name \
				  ], [], [AC_MSG_ERROR([$ac_have required but not found. Symbol provided by libssl, try installing libssl-dev or similar packages.])],[[ \
				  																																		 #include <openssl/ssl.h> \
				  																																		 #include <openssl/evp.h> \
				  																																		 #include <openssl/tls1.h> \  
				  																																		 ]] )
				   
AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT

