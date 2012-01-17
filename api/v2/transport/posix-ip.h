/* Copyright (C) 2011-2012 GSI GmbH.
 *
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * This provides common methods for UDP/TCP.
 */

#ifndef EB_POSIX_IP_H
#define EB_POSIX_IP_H

#include <sys/types.h>
#include <sys/socket.h>

typedef int eb_posix_sock_t;

void eb_posix_ip_close(eb_posix_sock_t sock);
eb_posix_sock_t eb_posix_ip_open(int type, int port);
socklen_t eb_posix_ip_resolve(const char* prefix, const char* address, int type, struct sockaddr_storage* out);

#endif
