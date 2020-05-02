
# Initate configure.ac 

AC_INIT(MID,[1.4],,,[https://github.com/m0hithreddy/MID])

# Safety check in case user overwritten --srcdir

AC_CONFIG_SRCDIR([src/MID.c])

AC_CONFIG_AUX_DIR([build-aux])

# Checks for source files and other important files

AC_CHECK_FILES(
[${srcdir}/src/MID.c
${srcdir}/src/MID.h
${srcdir}/src/MID_http.c
${srcdir}/src/MID_http.h
${srcdir}/src/MID_arguments.c
${srcdir}/src/MID_arguments.h
${srcdir}/src/MID_interfaces.c
${srcdir}/src/MID_interfaces.h
${srcdir}/src/MID_socket.c
${srcdir}/src/MID_socket.h
${srcdir}/src/MID_ssl_socket.c
${srcdir}/src/MID_ssl_socket.h
${srcdir}/src/MID_structures.c
${srcdir}/src/MID_structures.h
${srcdir}/src/MID_unit.c
${srcdir}/src/MID_unit.h
${srcdir}/src/MID_functions.c
${srcdir}/src/MID_functions.h
${srcdir}/src/MID_state.c
${srcdir}/src/MID_state.h
${srcdir}/src/MID_err.h
${srcdir}/src/url_parser.c
${srcdir}/src/url_parser.h
${srcdir}/src/Makefile.am
${srcdir}/Makefile.am
${srcdir}/MID.conf],
[],
[AC_MSG_ERROR([Repository corrupted, redownload the source.])]
)

# Checks for necessary programs

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

# Checks for system headers

AC_CHECK_HEADERS(
[stdio.h \
stdlib.h \
stddef.h \
string.h \
unistd.h \
netdb.h \
fcntl.h \
netinet/in.h \
sys/socket.h \
sys/file.h \
sys/stat.h \
sys/types.h \
sys/select.h \
sys/signalfd.h \
sys/time.h \
arpa/inet.h \
limits.h \
bits/sockaddr.h \
arpa/inet.h \
ifaddrs.h \
linux/if_link.h \
errno.h \
time.h \
pthread.h \
signal.h \
ctype.h],
[],
[AC_MSG_ERROR([${ac_header} is required but not found. Headers provided by libc, installed by default in all distributions.])]
)

# Checks for system functions

AC_CHECK_FUNCS(
[printf \
strcmp \
getuid \
geteuid \
fprintf \
snprintf \
sprintf \
pwrite \
pread \ 
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
strdup \
bzero \
fileno \
fopen \
fclose \
flock \
fstat \
lseek \
ftruncate \
remove \
time \
memcpy \
memset \
memcmp \
sleep \
read \
write \
fcntl \
select \
signalfd \
getifaddrs \
getnameinfo \
getaddrinfo \
htons \
inet_pton \
socket \
setsockopt \
bind \
connect \
close \
shutdown \
inet_ntop \
sigemptyset \
sigaddset \
sigwait \
sigtimedwait \
isalpha \
tolower],
[],
[AC_MSG_ERROR([${ac_func} is required but not found. Should be included in libc, check the libc variant installed in your system.])]
)

#Checks for optional system functions

AC_CHECK_FUNCS(
[memndup],
[],
[] 
)

# Checks for system symbols

AC_CHECK_DECLS(
[NULL,
LONG_MAX,
LONG_MIN,
LOCK_EX,
LOCK_UN,
LOCK_NB,
SEEK_SET,
SIGINT,
SIGQUIT,
SIGRTMIN,
SIG_BLOCK,
F_GETFL,
F_SETFL,
FD_ZERO,
FD_SET,
FD_ISSET,
EAGAIN,
EWOULDBLOCK,
EINTR,
AF_INET,
AF_INET6,
SOCK_STREAM,
IPPROTO_TCP,
AI_ADDRCONFIG,
INET_ADDRSTRLEN,
INET6_ADDRSTRLEN],
[],
[AC_MSG_ERROR([Symbol required but not found. Should be included in libc, check the libc variant installed in your system.])],
[[#include<stddef.h>
#include<limits.h>
#include<sys/file.h>
#include<errno.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<signal.h>
#include<netdb.h>]]
)

# Pthread library checks

AC_CHECK_LIB(pthread,pthread_create, [], [AC_MSG_ERROR([-lpthread is required but not found. Installed by default in all distributions])])

AC_CHECK_FUNCS(
[pthread_create \
pthread_join \
pthread_mutex_init \
pthread_mutex_lock \
pthread_mutex_unlock \
pthread_mutex_destroy \
pthread_kill \
pthread_sigmask],
[],
[AC_MSG_ERROR([${ac_func} required but not found. Function provided by libpthread, installed by default in all distributions])]
)

#SSL library checks

AC_ARG_WITH([ssl], AC_HELP_STRING([--without-ssl],[disable SSL support]), [], [with_ssl=yes])

if test "x${with_ssl}" == "xyes" ; then 

AC_CHECK_LIB(crypto,BIO_new,, [])

AC_CHECK_LIB(ssl,SSL_new,, [], [-lcrypto] ) 

if test "x${ac_cv_lib_crypto_BIO_new}" == "xyes" && test "x${ac_cv_lib_ssl_SSL_new}" == "xyes" ; then

AC_CHECK_HEADERS(
[openssl/ssl.h \
openssl/err.h \
openssl/evp.h \
openssl/tls1.h \
openssl/md5.h],
[],
[ssl_header_not_found="yes"]
)

AC_CHECK_FUNCS(
[TLS_method \
SSL_CTX_new \
SSL_new \
SSL_set_fd \
SSL_connect \
SSL_write \
SSL_read \
MD5_Init \
MD5_Update \
MD5_Final \
SSL_get_error \
ERR_get_error],
[],
[ssl_func_not_found="yes"]
)

AC_CHECK_DECLS(
[SSL_library_init,
OpenSSL_add_all_algorithms,
SSL_load_error_strings,
SSL_set_tlsext_host_name,
MD5_DIGEST_LENGTH,
SSL_ERROR_WANT_READ,
SSL_ERROR_WANT_WRITE,
SSL_ERROR_SYSCALL,
SSL_ERROR_ZERO_RETURN],
[],
[ssl_symbol_not_found="yes"],
[[#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/tls1.h>
#include<openssl/md5.h>]]
)

if test "x${ssl_header_not_found}" != "xyes" && test "x${ssl_func_not_found}" != "xyes" && test "x${ssl_symbol_not_found}" != "xyes" ; then
AC_DEFINE([LIBSSL_SANE],[1],["libssl installed and found all required headers, functions and symbols"])
AC_MSG_NOTICE([libssl and libcrytpo sane, enabling SSL support...])
else
AC_MSG_NOTICE([libssl and libcrypto are installed, but required headers or functions or symbols not found. Disabling the SSL support!])
fi

else
AC_MSG_NOTICE([libssl or libcrypto not found! Support for SSL is disabled, try installing libssl-dev or similar packages and recompile the source for enabling the SSL support])
fi

else
AC_MSG_NOTICE([Disabling SSL support as per user requested!])
fi

# Zlib checks

AC_ARG_WITH([z], AC_HELP_STRING([--without-z],[disable GZIP support]), [], [with_z=yes])

if test "x${with_z}" == "xyes" ; then

AC_CHECK_LIB(z,inflate,[],[AC_MSG_NOTICE([libz not found! Support for gzipped encodings is disabled, try installing zlib1g-dev or similar packages and recompile the source for enabling the GZIP support])]) 

if test "x${ac_cv_lib_z_inflate}" == "xyes" ; then

AC_CHECK_HEADERS(
[zlib.h],
[],
[zlib_header_not_found=yes])

AC_CHECK_FUNCS(
[inflate \
inflateEnd],
[],
[zlib_func_not_found=yes])

AC_CHECK_DECLS(
[inflateInit2,
Z_FINISH,
Z_DATA_ERROR,
Z_NEED_DICT,
Z_MEM_ERROR,
Z_STREAM_END],
[],
[zlib_symbol_not_found="yes"],
[[#include <zlib.h>]]
)

if test "x${zlib_header_not_found}" != "xyes" && test "x${zlib_func_not_found}" != "xyes" && test "x${zlib_symbol_not_found}" != "xyes" ; then
AC_DEFINE([LIBZ_SANE],[1],["libz installed and found all required headers, functions and symbols"])
AC_MSG_NOTICE([libz sane, enabling GZIP support...])
else
AC_MSG_NOTICE([libz installed, but required headers or functions or symbols not found. Disabling the GZIP support!])
fi 

fi

else
AC_MSG_NOTICE([Disabling GZIP support as per user requested!])
fi

# System Architecture

AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([HOST_ARCH], ["$host_os"], ["Host architecture"])
 
# Conclude configure.ac 

AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT
