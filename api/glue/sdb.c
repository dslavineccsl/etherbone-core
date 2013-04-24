/** @file sdb.c
 *  @brief Implement the SDB data structure on the local bus.
 *
 *  Copyright (C) 2011-2012 GSI Helmholtz Centre for Heavy Ion Research GmbH 
 *
 *  We reserved the low 8K memory region for this device.
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
#define EB_NEED_BIGENDIAN_64 1

#include "socket.h"
#include "sdb.h"
#include "version.h"
#include "../format/bigendian.h"
#include "../memory/memory.h"

#include <string.h>

#define SDB_MAGIC 0x5344422D

static eb_data_t eb_sdb_extract(void* data, eb_width_t width, eb_address_t addr) {
  eb_data_t out;
  uint8_t* bytes = (uint8_t*)data;
  eb_width_t i;
  
  out = 0;
  width &= EB_DATAX;
  for (i = 0; i < width; ++i) {
    out <<= 8;
    out |= bytes[addr+i];
  }
  
  return out;
}

static eb_data_t eb_sdb_interconnect(eb_width_t width, eb_address_t addr, int devices) {
  struct sdb_interconnect interconnect;
  eb_data_t out;
  
  interconnect.sdb_magic    = htobe32(SDB_MAGIC);
  interconnect.sdb_records  = htobe16(devices+1);
  interconnect.sdb_version  = 1;
  interconnect.sdb_bus_type = sdb_wishbone;
  
  interconnect.sdb_component.addr_first = htobe64(0);
  interconnect.sdb_component.addr_last  = htobe64(~(eb_address_t)0);
  
  interconnect.sdb_component.product.vendor_id  = htobe64(0x651); /* GSI */
  interconnect.sdb_component.product.device_id  = htobe32(0x02398114);
  interconnect.sdb_component.product.version    = htobe32(EB_VERSION_SHORT);
  interconnect.sdb_component.product.date       = htobe32(EB_DATE_SHORT);
  interconnect.sdb_component.product.record_type = sdb_interconnect;

  memcpy(&interconnect.sdb_component.product.name[0], "Software-EB-Bus    ", sizeof(interconnect.sdb_component.product.name));

  /* Extract the value needed */
  out = eb_sdb_extract(&interconnect, width, addr);
  
  return out;
}

static eb_data_t eb_sdb_device(sdb_device_t device, eb_width_t width, eb_address_t addr) {
  struct sdb_device dev;
  
  dev.abi_class     = htobe16(device->abi_class);
  dev.abi_ver_major = device->abi_ver_major;
  dev.abi_ver_minor = device->abi_ver_minor;
  dev.bus_specific  = htobe32(device->bus_specific);
  
  dev.sdb_component.addr_first = htobe64(device->sdb_component.addr_first);
  dev.sdb_component.addr_last  = htobe64(device->sdb_component.addr_last);
  
  dev.sdb_component.product.vendor_id   = htobe64(device->sdb_component.product.vendor_id);
  dev.sdb_component.product.device_id   = htobe32(device->sdb_component.product.device_id);
  dev.sdb_component.product.version     = htobe32(device->sdb_component.product.version);
  dev.sdb_component.product.date        = htobe32(device->sdb_component.product.date);
  dev.sdb_component.product.record_type = sdb_device;
  
  memcpy(&dev.sdb_component.product.name[0], &device->sdb_component.product.name[0], sizeof(dev.sdb_component.product.name));
  
  return eb_sdb_extract(&dev, width, addr);
}

eb_data_t eb_sdb(eb_socket_t socketp, eb_width_t width, eb_address_t addr) {
  struct eb_socket* socket;
  struct eb_handler_address* address;
  eb_handler_address_t addressp;
  int dev;
  
  socket = EB_SOCKET(socketp);
  
  if (addr < 0x40) {
    /* Count the devices */
    dev = 0;
    for (addressp = socket->first_handler; addressp != EB_NULL; addressp = address->next) {
      address = EB_HANDLER_ADDRESS(addressp);
      ++dev;
    }
    return eb_sdb_interconnect(width, addr, dev);
  }
  
  dev = addr >> 6;
  addr &= 0x3f;
  
  for (addressp = socket->first_handler; addressp != EB_NULL; addressp = address->next) {
    address = EB_HANDLER_ADDRESS(addressp);
    if (--dev == 0) break;
  }
  
  if (addressp == EB_NULL) return 0;
  return eb_sdb_device(address->device, width, addr);
}

static int eb_sdb_fill_block(uint8_t* ptr, uint16_t max_len, eb_operation_t ops) {
  eb_data_t data;
  uint8_t* eptr;
  int i, stride;
  
  for (eptr = ptr + max_len; ops != EB_NULL; ops = eb_operation_next(ops)) {
    if (eb_operation_had_error(ops)) return -1;
    data = eb_operation_data(ops);
    stride = eb_operation_format(ops) & EB_DATAX;
    
    /* More data follows */
    if (eptr-ptr < stride) return 1;
    
    for (i = stride-1; i >= 0; --i) {
      ptr[i] = data & 0xFF;
      data >>= 8;
    }
    ptr += stride;
  }
  
  return 0;
}

static void eb_sdb_product_decode(struct sdb_product* product) {
  product->vendor_id  = be64toh(product->vendor_id);
  product->device_id  = be32toh(product->device_id);
  product->version    = be32toh(product->version);
  product->date       = be32toh(product->date);
}

static void eb_sdb_component_decode(struct sdb_component* component, eb_address_t bus_base) {
  component->addr_first = be64toh(component->addr_first) + bus_base;
  component->addr_last  = be64toh(component->addr_last)  + bus_base;
  eb_sdb_product_decode(&component->product);
}

static void eb_sdb_decode(struct eb_sdb_scan* scan, eb_device_t device, uint8_t* buf, uint16_t size, eb_operation_t ops) {
  eb_user_data_t data;
  sdb_callback_t cb;
  eb_address_t bus_base;
  sdb_t sdb;
  uint16_t i;
  
  cb = scan->cb;
  data = scan->user_data;
  bus_base = scan->bus_base;
  
  if (eb_sdb_fill_block(buf, size, ops) != 0) {
    (*cb)(data, device, 0, EB_FAIL);
    return;
  }
  
  sdb = (sdb_t)buf;
  
  /* Bus endian fixup */
  sdb->interconnect.sdb_magic    = be32toh(sdb->interconnect.sdb_magic);
  sdb->interconnect.sdb_records  = be16toh(sdb->interconnect.sdb_records);
  eb_sdb_component_decode(&sdb->interconnect.sdb_component, bus_base);

  if (sizeof(struct sdb_device) * sdb->interconnect.sdb_records > size) {
    (*cb)(data, device, 0, EB_FAIL);
    return;
  }
  
  /* Descriptor blocks */
  for (i = 0; i < sdb->interconnect.sdb_records-1; ++i) {
    sdb_record_t r = &sdb->record[i];
    
    switch (r->empty.record_type) {
    case sdb_device:
      r->device.abi_class    = be16toh(r->device.abi_class);
      r->device.bus_specific = be32toh(r->device.bus_specific);
      eb_sdb_component_decode(&r->device.sdb_component, bus_base);
      break;
    
    case sdb_bridge:
      r->bridge.sdb_child = be64toh(r->bridge.sdb_child) + bus_base;
      eb_sdb_component_decode(&r->bridge.sdb_component, bus_base);
      break;
    
    case sdb_integration:
      eb_sdb_product_decode(&r->integration.product);
      break;
      
    case sdb_empty:
      break;
      
    default:
      /* unknown record; skip */
      break;
    }
  }
  
  (*cb)(data, device, sdb, EB_OK);
}

/* We allocate buffer on the stack to hack around missing alloca */
#define EB_SDB_DECODE(x)                                                                          \
static void eb_sdb_decode##x(struct eb_sdb_scan* scan, eb_device_t device, eb_operation_t ops) {   \
  union {                                                                                          \
    struct {                                                                                       \
      struct sdb_interconnect interconnect;                                                        \
      union sdb_record record[x];                                                                  \
    } s;                                                                                           \
    uint8_t bytes[1];                                                                              \
  } sdb;                                                                                           \
  return eb_sdb_decode(scan, device, &sdb.bytes[0], sizeof(sdb), ops);                             \
}

EB_SDB_DECODE(4)
EB_SDB_DECODE(8)
EB_SDB_DECODE(16)
EB_SDB_DECODE(32)
EB_SDB_DECODE(64)
EB_SDB_DECODE(128)
EB_SDB_DECODE(256)

static void eb_sdb_got_all(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  union {
    struct sdb_interconnect interconnect;
    uint8_t bytes[1];
  } header;
  struct eb_sdb_scan* scan;
  eb_sdb_scan_t scanp;
  eb_user_data_t data;
  sdb_callback_t cb;
  uint16_t devices;
  
  scanp = (eb_sdb_scan_t)(uintptr_t)mydata;
  scan = EB_SDB_SCAN(scanp);
  cb = scan->cb;
  data = scan->user_data;
  
  if (status != EB_OK) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, status);
    return;
  }
  
  if (eb_sdb_fill_block(&header.bytes[0], sizeof(header), ops) != 1) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, EB_FAIL);
    return;
  }
  
  devices = be16toh(header.interconnect.sdb_records) - 1;
  
  if      (devices <   4) eb_sdb_decode4(scan, device, ops);
  else if (devices <   8) eb_sdb_decode8(scan, device, ops);
  else if (devices <  16) eb_sdb_decode16(scan, device, ops);
  else if (devices <  32) eb_sdb_decode32(scan, device, ops);
  else if (devices <  64) eb_sdb_decode64(scan, device, ops);
  else if (devices < 128) eb_sdb_decode128(scan, device, ops);
  else if (devices < 256) eb_sdb_decode256(scan, device, ops);
  else (*cb)(data, device, 0, EB_OOM);
  
  eb_free_sdb_scan(scanp);
}  

static void eb_sdb_got_header(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  union {
    struct sdb_interconnect interconnect;
    uint8_t bytes[1];
  } header;
  struct eb_sdb_scan* scan;
  eb_sdb_scan_t scanp;
  eb_user_data_t data;
  sdb_callback_t cb;
  eb_address_t address, end;
  eb_cycle_t cycle;
  int stride;
  
  scanp = (eb_sdb_scan_t)(uintptr_t)mydata;
  scan = EB_SDB_SCAN(scanp);
  cb = scan->cb;
  data = scan->user_data;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  if (status != EB_OK) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, status);
    return;
  }
  
  /* Read in the header */
  if (eb_sdb_fill_block(&header.bytes[0], sizeof(header), ops) != 0) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, EB_FAIL);
    return;
  }
  
  /* Is the magic there? */
  if (be32toh(header.interconnect.sdb_magic) != SDB_MAGIC) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, EB_FAIL);
    return;
  }
  
  /* Now, we need to read: entire table */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_all, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, status);
    return;
  }
  
  /* Read: header again */
  address = eb_operation_address(ops);
  for (end = address + (((eb_address_t)be16toh(header.interconnect.sdb_records)) << 6); address < end; address += stride)
    eb_cycle_read(cycle, address, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
}

eb_status_t eb_sdb_scan_bus(eb_device_t device, sdb_bridge_t bridge, eb_user_data_t data, sdb_callback_t cb) {
  struct eb_sdb_scan* scan;
  eb_cycle_t cycle;
  eb_sdb_scan_t scanp;
  int stride;
  eb_status_t status;
  eb_address_t header_address;
  eb_address_t header_end;
  
  if (bridge->sdb_component.product.record_type != sdb_bridge)
    return EB_ADDRESS;
  
  if ((scanp = eb_new_sdb_scan()) == EB_NULL)
    return EB_OOM;
  
  scan = EB_SDB_SCAN(scanp);
  scan->cb = cb;
  scan->user_data = data;
  scan->bus_base = bridge->sdb_component.addr_first;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  /* scan invalidated by all the EB calls below (which allocate) */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    return status;
  }
  
  header_address = bridge->sdb_child;
  for (header_end = header_address + 32; header_address < header_end; header_address += stride)
    eb_cycle_read(cycle, header_address, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
  
  return EB_OK;
}

static void eb_sdb_got_header_ptr(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  struct eb_sdb_scan* scan;
  eb_sdb_scan_t scanp;
  eb_user_data_t data;
  eb_address_t header_address;
  eb_address_t header_end;
  sdb_callback_t cb;
  eb_cycle_t cycle;
  int stride;
  
  scanp = (eb_sdb_scan_t)(uintptr_t)mydata;
  scan = EB_SDB_SCAN(scanp);
  cb = scan->cb;
  data = scan->user_data;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  if (status != EB_OK) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, status);
    return;
  }
  
  /* Calculate the address from partial reads */
  header_address = 0;
  for (; ops != EB_NULL; ops = eb_operation_next(ops)) {
    if (eb_operation_had_error(ops)) {
      eb_free_sdb_scan(scanp);
      (*cb)(data, device, 0, EB_FAIL);
      return;
    }
    header_address <<= (stride*8);
    header_address += eb_operation_data(ops);
  }
  
  /* Now, we need to read the header */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    (*cb)(data, device, 0, status);
    return;
  }
  
  for (header_end = header_address + 32; header_address < header_end; header_address += stride)
    eb_cycle_read(cycle, header_address, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
}

eb_status_t eb_sdb_scan_root(eb_device_t device, eb_user_data_t data, sdb_callback_t cb) {
  struct eb_sdb_scan* scan;
  eb_cycle_t cycle;
  eb_sdb_scan_t scanp;
  eb_status_t status;
  int addr, stride;
  
  if ((scanp = eb_new_sdb_scan()) == EB_NULL)
    return EB_OOM;
  
  scan = EB_SDB_SCAN(scanp);
  scan->cb = cb;
  scan->user_data = data;
  scan->bus_base = 0;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  /* scan invalidated by all the EB calls below (which allocate) */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header_ptr, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    return status;
  }
  
  for (addr = 8; addr < 16; addr += stride)
    eb_cycle_read_config(cycle, addr, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
  
  return EB_OK;
}
