/** @file transport.c
 *  @brief Bind all supported transports into one table.
 *
 *  Copyright (C) 2011-2012 GSI Helmholtz Centre for Heavy Ion Research GmbH 
 *
 *  All supported transports are included in the global eb_transports[].
 *
 *  @author Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 *  @bug None!
 *
 *******************************************************************************
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************
 */

#define ETHERBONE_IMPL

#include "transport.h"
#include "posix-udp.h"
#include "posix-tcp.h"
#include "ssh.h"

struct eb_transport_ops eb_transports[] = {
  {
    EB_POSIX_UDP_MTU,
    eb_posix_udp_open,
    eb_posix_udp_close,
    eb_posix_udp_connect,
    eb_posix_udp_disconnect,
    eb_posix_udp_fdes,
    eb_posix_udp_poll,
    eb_posix_udp_recv,
    eb_posix_udp_send
  },
  {
    EB_POSIX_TCP_MTU,
    eb_posix_tcp_open,
    eb_posix_tcp_close,
    eb_posix_tcp_connect,
    eb_posix_tcp_disconnect,
    eb_posix_tcp_fdes,
    eb_posix_tcp_poll,
    eb_posix_tcp_recv,
    eb_posix_tcp_send
  },
  {
    EB_SSH_MTU,
    eb_ssh_open,
    eb_ssh_close,
    eb_ssh_connect,
    eb_ssh_disconnect,
    eb_ssh_fdes,
    eb_ssh_poll,
    eb_ssh_recv,
    eb_ssh_send
  }
};

const unsigned int eb_transport_size = sizeof(eb_transports) / sizeof(struct eb_transport_ops);