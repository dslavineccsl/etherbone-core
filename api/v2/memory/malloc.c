/* Copyright (C) 2011-2012 GSI GmbH.
 *
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * This implements Etherbone memory using C malloc.
 */

#ifdef EB_USE_MALLOC

#include <stdlib.h>
#include "memory.h"

eb_cycle_operation_t  eb_new_cycle_operation (void) { return (eb_cycle_operation_t) malloc(sizeof(struct eb_cycle_operation));  }
eb_cycle_t            eb_new_cycle           (void) { return (eb_cycle_t)           malloc(sizeof(struct eb_cycle));            }
eb_device_t           eb_new_device          (void) { return (eb_device_t)          malloc(sizeof(struct eb_device));           }
eb_handler_callback_t eb_new_handler_callback(void) { return (eb_handler_callback_t)malloc(sizeof(struct eb_handler_callback)); }
eb_handler_address_t  eb_new_handler_address (void) { return (eb_handler_address_t) malloc(sizeof(struct eb_handler_address));  }
eb_response_t         eb_new_response        (void) { return (eb_response_t)        malloc(sizeof(struct eb_response));         }
eb_socket_t           eb_new_socket          (void) { return (eb_socket_t)          malloc(sizeof(struct eb_socket));           }

void eb_free_cycle_operation (eb_cycle_operation_t  x) { free(x); }
void eb_free_cycle           (eb_cycle_t            x) { free(x); }
void eb_free_device          (eb_device_t           x) { free(x); }
void eb_free_handler_callback(eb_handler_callback_t x) { free(x); }
void eb_free_handler_address (eb_handler_address_t  x) { free(x); }
void eb_free_response        (eb_response_t         x) { free(x); }
void eb_free_socket          (eb_socket_t           x) { free(x); }

#endif
