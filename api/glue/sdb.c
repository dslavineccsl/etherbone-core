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
  interconnect.sdb_component.product.version    = htobe32(eb_version_short);
  interconnect.sdb_component.product.date       = htobe32(eb_date_short);
  interconnect.sdb_component.product.record_type = sdb_record_interconnect;

  memcpy(&interconnect.sdb_component.product.name[0], "Software-EB-Bus    ", sizeof(interconnect.sdb_component.product.name));

  /* Extract the value needed */
  out = eb_sdb_extract(&interconnect, width, addr);
  
  return out;
}

static eb_data_t eb_sdb_device(const struct sdb_device* device, eb_width_t width, eb_address_t addr) {
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
  dev.sdb_component.product.record_type = sdb_record_device;
  
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
  eb_operation_t op;
  eb_data_t data;
  eb_address_t base;
  eb_address_t address;
  int i, stride;
  
  base = -1;
  for (op = ops; op != EB_NULL; op = eb_operation_next(op)) {
    if (eb_operation_had_error(op)) return -1;
    address = eb_operation_address(op);
    if (address < base) base = address;
  }
  
  for (op = ops; op != EB_NULL; op = eb_operation_next(op)) {
    address = eb_operation_address(op);
    data = eb_operation_data(op);
    stride = eb_operation_format(ops) & EB_DATAX;
    
    address -= base;
    if (address + stride > max_len) return 1;
    
    for (i = stride-1; i >= 0; --i) {
      ptr[address + i] = data & 0xFF;
      data >>= 8;
    }
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

static void eb_sdb_decode(eb_sdb_scan_t scanp, eb_device_t device, uint8_t* buf, uint16_t size, eb_operation_t ops) {
  eb_user_data_t data;
  sdb_callback_t cb;
  sdb_callback_msi_t cb_msi;
  uint16_t cb_fmt;
  eb_address_t bus_base;
  eb_address_t msi_first;
  eb_address_t msi_last;
  struct sdb_table* sdb;
  struct eb_sdb_scan* scan;
  struct eb_sdb_scan_meta* meta;
  uint16_t i;
  int found_msi;
  
  scan = EB_SDB_SCAN(scanp);
  meta = EB_SDB_SCAN_META(scan->meta);
  
  cb = scan->un_cb.cb;
  cb_msi = scan->un_cb.cb_msi;
  cb_fmt = scan->cb_fmt;
  data = scan->user_data;
  bus_base = meta->bus_base;
  msi_first = meta->msi_first;
  msi_last = meta->msi_last;
  
  if (eb_sdb_fill_block(buf, size, ops) != 0) {
    if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
    return;
  }
  
  sdb = (struct sdb_table*)buf;
  
  /* Bus endian fixup */
  sdb->interconnect.sdb_magic    = be32toh(sdb->interconnect.sdb_magic);
  sdb->interconnect.sdb_records  = be16toh(sdb->interconnect.sdb_records);
  eb_sdb_component_decode(&sdb->interconnect.sdb_component, bus_base);

  if (sizeof(struct sdb_device) * sdb->interconnect.sdb_records > size) {
    if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
    return;
  }
  
  /* Descriptor blocks */
  found_msi = 0;
  for (i = 0; i < sdb->interconnect.sdb_records-1; ++i) {
    union sdb_record* r = &sdb->record[i];
    
    switch (r->empty.record_type) {
    case sdb_record_device:
      r->device.abi_class    = be16toh(r->device.abi_class);
      r->device.bus_specific = be32toh(r->device.bus_specific);
      eb_sdb_component_decode(&r->device.sdb_component, bus_base);
      break;
    
    case sdb_record_bridge:
      r->bridge.sdb_child = be64toh(r->bridge.sdb_child) + bus_base;
      eb_sdb_component_decode(&r->bridge.sdb_component, bus_base);
      break;
    
    case sdb_record_msi:
      r->msi.msi_flags    = be32toh(r->msi.msi_flags);
      r->msi.bus_specific = be32toh(r->msi.bus_specific);
      /* For MSI, do NOT rebase the nested records as the relationship is inverted */
      eb_sdb_component_decode(&r->msi.sdb_component, 0);
      
      /* However, DO compute the MSI recursively */
      if ((r->msi.msi_flags >> 31) != 0) {
        /* The first time we find a matching master, that sets last for the rest */
        if (found_msi) {
          /* Two matching records?! Invalid SDB. */
          if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
          if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
          return;
        }
        found_msi = 1;
        /* If parent SDB record said we have no MSI, we still don't */
        if (msi_first <= msi_last) {
          msi_first += r->msi.sdb_component.addr_first;
          /* First SDB we see sets the size of our MSI result */
          if (msi_last == 0) {
            msi_last = r->msi.sdb_component.addr_last;
          } else {
            msi_last += r->msi.sdb_component.addr_first;
          }
        }
      }
      break;
    
    case sdb_record_integration:
      eb_sdb_product_decode(&r->integration.product);
      break;
      
    case sdb_record_empty:
      break;
      
    default:
      /* unknown record; skip */
      break;
    }
  }
  
  /* Mark an invalid range if not MSI capable */
  if (!found_msi) {
    msi_first = ~(eb_address_t)0;
    msi_last  = 0;
  }
  
  if (cb_fmt == 1) (*cb)(data, device, sdb, EB_OK);
  if (cb_fmt == 2) (*cb_msi)(data, device, sdb, msi_first, msi_last, EB_OK);
}

/* We allocate buffer on the stack to hack around missing alloca */
#define EB_SDB_DECODE(x)                                                                           \
static void eb_sdb_decode##x(eb_sdb_scan_t scan, eb_device_t device, eb_operation_t ops) {   \
  union {                                                                                          \
    struct {                                                                                       \
      struct sdb_interconnect interconnect;                                                        \
      union sdb_record record[x];                                                                  \
    } s;                                                                                           \
    uint8_t bytes[1];                                                                              \
  } sdb;                                                                                           \
  eb_sdb_decode(scan, device, &sdb.bytes[0], sizeof(sdb), ops);                                    \
}

EB_SDB_DECODE(4)
EB_SDB_DECODE(8)
EB_SDB_DECODE(16)
EB_SDB_DECODE(32)
EB_SDB_DECODE(64)
EB_SDB_DECODE(128)
EB_SDB_DECODE(256)

static void eb_sdb_got_record(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  struct eb_sdb_record* record;
  struct eb_sdb_scan* scan;
  struct eb_operation* op;
  struct eb_operation* op2;
  struct eb_operation* opi;
  eb_sdb_record_t recordp;
  eb_sdb_scan_t scanp;
  eb_sdb_scan_meta_t metap;
  eb_operation_t op2p;
  eb_operation_t opip;
  eb_user_data_t data;
  sdb_callback_t cb;
  sdb_callback_msi_t cb_msi;
  uint16_t cb_fmt;
  uint16_t devices;
  
  recordp = (eb_sdb_record_t)(uintptr_t)mydata;
  record = EB_SDB_RECORD(recordp);

  if (status != EB_OK) {
    record->status = status;
  } else if (ops == EB_NULL) {
    record->status = EB_FAIL;
  } else if ((op2p = eb_new_operation()) == EB_NULL) {
    record = EB_SDB_RECORD(recordp);
    record->status = EB_OOM;
  } else {
    record = EB_SDB_RECORD(recordp);
    op = EB_OPERATION(ops);
    op2 = EB_OPERATION(op2p);
    
    /* Clone the operation */
    *op2 = *op;
    /* Steal its children */
    op->next = EB_NULL;
    
    /* Find the last operation */
    opip = op2p;
    opi = EB_OPERATION(opip);
    while (opi->next != EB_NULL) {
      opip = opi->next;
      opi = EB_OPERATION(opip);
    }
    
    /* Glue with previous list */
    opi->next = record->ops;
    record->ops = op2p;
  }
  
  if (--record->pending == 0) {
    scanp = record->scan;
    scan = EB_SDB_SCAN(scanp);
    metap = scan->meta;
    cb = scan->un_cb.cb;
    cb_msi = scan->un_cb.cb_msi;
    cb_fmt = scan->cb_fmt;
    data = scan->user_data;
    
    if (record->status != EB_OK) {
      if (cb_fmt == 1) (*cb)(data, device, 0, record->status);
      if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, record->status);
    } else {
      devices = record->records - 1;
      
      if      (devices <   4) eb_sdb_decode4(scanp, device, record->ops);
      else if (devices <   8) eb_sdb_decode8(scanp, device, record->ops);
      else if (devices <  16) eb_sdb_decode16(scanp, device, record->ops);
      else if (devices <  32) eb_sdb_decode32(scanp, device, record->ops);
      else if (devices <  64) eb_sdb_decode64(scanp, device, record->ops);
      else if (devices < 128) eb_sdb_decode128(scanp, device, record->ops);
      else if (devices < 256) eb_sdb_decode256(scanp, device, record->ops);
      else {
        if (cb_fmt == 1) (*cb)(data, device, 0, EB_OOM);
        if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_OOM);
      }
    }
    
    /* Free everything */
    record = EB_SDB_RECORD(recordp);
    for (opip = record->ops; opip != EB_NULL; opip = op2p) {
      op2p = EB_OPERATION(opip)->next;
      eb_free_operation(opip);
    }
    
    eb_free_sdb_record(recordp);
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
  }
}

static void eb_sdb_got_header(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  union {
    struct sdb_interconnect interconnect;
    uint8_t bytes[1];
  } header;
  struct eb_sdb_scan* scan;
  struct eb_sdb_record* record;
  eb_sdb_scan_t scanp;
  eb_sdb_scan_meta_t metap;
  eb_sdb_record_t recordp;
  eb_user_data_t data;
  sdb_callback_t cb;
  sdb_callback_msi_t cb_msi;
  uint16_t cb_fmt;
  eb_address_t address, end;
  eb_cycle_t cycle;
  int stride;
  uint16_t i, records;
  
  scanp = (eb_sdb_scan_t)(uintptr_t)mydata;
  scan = EB_SDB_SCAN(scanp);
  cb = scan->un_cb.cb;
  cb_msi = scan->un_cb.cb_msi;
  cb_fmt = scan->cb_fmt;
  data = scan->user_data;
  
  metap = scan->meta;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  if (status != EB_OK) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, status);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, status);
    return;
  }
  
  /* Read in the header */
  if (eb_sdb_fill_block(&header.bytes[0], sizeof(header), ops) != 0) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
    return;
  }
  
  /* Is the magic there? */
  if (be32toh(header.interconnect.sdb_magic) != SDB_MAGIC) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
    return;
  }
  
  /* Allocate a new record */
  if ((recordp = eb_new_sdb_record()) == EB_NULL) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, EB_OOM);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_OOM);
    return;
  }
  
  /* Invalidate by the allocate above: scan = EB_SDB_SCAN(scanp); */
  records = be16toh(header.interconnect.sdb_records);
  
  record = EB_SDB_RECORD(recordp);
  record->scan = scanp;
  record->ops = EB_NULL;
  record->status = EB_OK;
  record->pending = records;
  record->records = records;
  
  /* Now, we need to read: entire table */
  address = eb_operation_address(ops);
  for (i = 0; i < records; ++i) {
    if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)recordp, &eb_sdb_got_record, &cycle)) != EB_OK) {
      eb_sdb_got_record((eb_user_data_t)(uintptr_t)recordp, device, EB_NULL, status);
      continue;
    }
    
    /* Read record */
    for (end = address + 64; address < end; address += stride)
      eb_cycle_read(cycle, address, EB_DATAX, 0);
    
    eb_cycle_close(cycle);
  }
}

static eb_status_t eb_sdb_scan_bus_real(eb_device_t device, const struct sdb_bridge* bridge, eb_address_t msi_first, eb_address_t msi_last, eb_user_data_t data, sdb_callback_t cb, sdb_callback_msi_t cb_msi) {
  struct eb_sdb_scan* scan;
  struct eb_sdb_scan_meta* meta;
  eb_cycle_t cycle;
  eb_sdb_scan_t scanp;
  eb_sdb_scan_meta_t metap;
  int stride;
  eb_status_t status;
  eb_address_t header_address;
  eb_address_t header_end;
  
  if (bridge->sdb_component.product.record_type != sdb_record_bridge)
    return EB_ADDRESS;
  
  if ((scanp = eb_new_sdb_scan()) == EB_NULL)
    return EB_OOM;
  if ((metap = eb_new_sdb_scan_meta()) == EB_NULL) {
    eb_free_sdb_scan(scanp);
    return EB_OOM;
  }
  
  scan = EB_SDB_SCAN(scanp);
  if (cb) {
    scan->un_cb.cb = cb;
    scan->cb_fmt = 1;
  } else {
    scan->un_cb.cb_msi = cb_msi;
    scan->cb_fmt = 2;
  }
  scan->user_data = data;
  scan->meta = metap;
  
  meta = EB_SDB_SCAN_META(metap);
  meta->bus_base = bridge->sdb_component.addr_first;
  meta->msi_first = msi_first;
  meta->msi_last = msi_last;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  /* scan invalidated by all the EB calls below (which allocate) */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    return status;
  }
  
  header_address = bridge->sdb_child;
  for (header_end = header_address + 32; header_address < header_end; header_address += stride)
    eb_cycle_read(cycle, header_address, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
  
  return EB_OK;
}

eb_status_t eb_sdb_scan_bus(eb_device_t device, const struct sdb_bridge* bridge, eb_user_data_t data, sdb_callback_t cb) {
  return eb_sdb_scan_bus_real(device, bridge, 0, 0, data, cb, 0);
}

eb_status_t eb_sdb_scan_bus_msi(eb_device_t device, const struct sdb_bridge* bridge, eb_address_t msi_first, eb_address_t msi_last, eb_user_data_t data, sdb_callback_msi_t cb) {
  return eb_sdb_scan_bus_real(device, bridge, msi_first, msi_last, data, 0, cb);
}

static void eb_sdb_got_header_ptr(eb_user_data_t mydata, eb_device_t device, eb_operation_t ops, eb_status_t status) {
  struct eb_sdb_scan* scan;
  eb_sdb_scan_t scanp;
  eb_sdb_scan_meta_t metap;
  eb_user_data_t data;
  eb_address_t header_address;
  eb_address_t header_end;
  sdb_callback_t cb;
  sdb_callback_msi_t cb_msi;
  uint16_t cb_fmt;
  eb_cycle_t cycle;
  int stride;
  
  scanp = (eb_sdb_scan_t)(uintptr_t)mydata;
  scan = EB_SDB_SCAN(scanp);
  cb = scan->un_cb.cb;
  cb_msi = scan->un_cb.cb_msi;
  cb_fmt = scan->cb_fmt;
  data = scan->user_data;
  
  metap = scan->meta;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  if (status != EB_OK) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, status);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, status);
    return;
  }
  
  /* Calculate the address from partial reads */
  header_address = 0;
  for (; ops != EB_NULL; ops = eb_operation_next(ops)) {
    if (eb_operation_had_error(ops)) {
      eb_free_sdb_scan(scanp);
      eb_free_sdb_scan_meta(metap);
      if (cb_fmt == 1) (*cb)(data, device, 0, EB_FAIL);
      if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, EB_FAIL);
      return;
    }
    header_address <<= (stride*8);
    header_address += eb_operation_data(ops);
  }
  
  /* Now, we need to read the header */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    if (cb_fmt == 1) (*cb)(data, device, 0, status);
    if (cb_fmt == 2) (*cb_msi)(data, device, 0, 0, 0, status);
    return;
  }
  
  for (header_end = header_address + 32; header_address < header_end; header_address += stride)
    eb_cycle_read(cycle, header_address, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
}

static eb_status_t eb_sdb_scan_root_real(eb_device_t device, eb_user_data_t data, sdb_callback_t cb, sdb_callback_msi_t cb_msi) {
  struct eb_sdb_scan* scan;
  struct eb_sdb_scan_meta* meta;
  eb_cycle_t cycle;
  eb_sdb_scan_t scanp;
  eb_sdb_scan_meta_t metap;
  eb_status_t status;
  int addr, stride;
  
  if ((scanp = eb_new_sdb_scan()) == EB_NULL)
    return EB_OOM;
  if ((metap = eb_new_sdb_scan_meta()) == EB_NULL) {
    eb_free_sdb_scan(scanp);
    return EB_OOM;
  }
  
  scan = EB_SDB_SCAN(scanp);
  if (cb) {
    scan->un_cb.cb = cb;
    scan->cb_fmt = 1;
  } else {
    scan->un_cb.cb_msi = cb_msi;
    scan->cb_fmt = 2;
  }
  scan->user_data = data;
  scan->meta = metap;
  
  meta = EB_SDB_SCAN_META(metap);
  meta->bus_base  = 0;
  meta->msi_first = 0;
  meta->msi_last  = 0;
  
  stride = (eb_device_width(device) & EB_DATAX);
  
  /* scan invalidated by all the EB calls below (which allocate) */
  if ((status = eb_cycle_open(device, (eb_user_data_t)(uintptr_t)scanp, &eb_sdb_got_header_ptr, &cycle)) != EB_OK) {
    eb_free_sdb_scan(scanp);
    eb_free_sdb_scan_meta(metap);
    return status;
  }
  
  for (addr = 8; addr < 16; addr += stride)
    eb_cycle_read_config(cycle, addr, EB_DATAX, 0);
  
  eb_cycle_close(cycle);
  
  return EB_OK;
}

eb_status_t eb_sdb_scan_root(eb_device_t device, eb_user_data_t data, sdb_callback_t cb) {
  return eb_sdb_scan_root_real(device, data, cb, 0);
}

eb_status_t eb_sdb_scan_root_msi(eb_device_t device, eb_user_data_t data, sdb_callback_msi_t cb) {
  return eb_sdb_scan_root_real(device, data, 0, cb);
}

struct eb_find_by_address {
  eb_address_t address;
  struct sdb_device* output;
  eb_address_t* msi_first;
  eb_address_t* msi_last;
  eb_status_t status;
};

static void eb_cb_find_by_address(eb_user_data_t data, eb_device_t dev, const struct sdb_table* sdb, eb_address_t msi_first, eb_address_t msi_last, eb_status_t status) {
  int i, devices;
  const union sdb_record* des;
  struct eb_find_by_address* record;
  
  record = (struct eb_find_by_address*)data;
  
  if (status != EB_OK) {
    record->status = status;
    return;
  }
  
  des = 0; /* silence warning */
  devices = sdb->interconnect.sdb_records - 1;
  for (i = 0; i < devices; ++i) {
    des = &sdb->record[i];
    
    if (des->empty.record_type == sdb_record_bridge && 
        des->bridge.sdb_component.addr_first <= record->address && record->address <= des->bridge.sdb_component.addr_last) {
      if ((status = eb_sdb_scan_bus_msi(dev, &des->bridge, msi_first, msi_last, data, &eb_cb_find_by_address)) != EB_OK) {
        record->status = status;
      }
      return;
    }
    
    if (des->empty.record_type == sdb_record_device && 
        des->device.sdb_component.addr_first <= record->address && record->address <= des->device.sdb_component.addr_last) {
      memcpy(record->output, des, sizeof(struct sdb_device));
      *record->msi_first = msi_first;
      *record->msi_last  = msi_last;
      record->status = EB_OK;
      return;
    }
    
    /* sdb_record_msi is not matched, because it is another address space */
  }
  
  /* nothing matched! */
  record->status = EB_ADDRESS;
}

eb_status_t eb_sdb_find_by_address_msi(eb_device_t device, eb_address_t address, struct sdb_device* output, eb_address_t* msi_first, eb_address_t* msi_last) {
  struct eb_find_by_address record;
  
  record.address   = address;
  record.output    = output;
  record.msi_first = msi_first;
  record.msi_last  = msi_last;
  
  if ((record.status = eb_sdb_scan_root_msi(device, &record, eb_cb_find_by_address)) == EB_OK) {
    record.status = 1;
    while (record.status > 0) {
      eb_socket_run(eb_device_socket(device), -1);
    }
  }
  
  return record.status;
}

eb_status_t eb_sdb_find_by_address(eb_device_t device, eb_address_t address, struct sdb_device* output) {
  eb_address_t null;
  return eb_sdb_find_by_address_msi(device, address, output, &null, &null);
}

struct eb_find_by_identity {
  uint64_t vendor_id;
  struct sdb_device* output;
  eb_address_t* output_first;
  eb_address_t* output_last;
  uint32_t device_id;
  int size_output;
  int size_msi;
  int fill_output;
  int fill_msi;
  int pending;
  eb_status_t status;
};

static void eb_cb_find_by_identity(eb_user_data_t data, eb_device_t dev, const struct sdb_table* sdb, eb_address_t msi_first, eb_address_t msi_last, eb_status_t status) {
  int i, devices;
  const union sdb_record* des;
  struct eb_find_by_identity* record;
  
  record = (struct eb_find_by_identity*)data;
  --record->pending;
  
  if (status != EB_OK) {
    record->status = status;
    return;
  }
  
  des = 0; /* silence warning */
  devices = sdb->interconnect.sdb_records - 1;
  for (i = 0; i < devices; ++i) {
    des = &sdb->record[i];
    
    if (des->empty.record_type == sdb_record_bridge) {
      if ((status = eb_sdb_scan_bus_msi(dev, &des->bridge, msi_first, msi_last, data, &eb_cb_find_by_identity)) == EB_OK) {
        ++record->pending;
      } else {
        record->status = status;
      }
    }
    
    if ((des->empty.record_type == sdb_record_device || 
         des->empty.record_type == sdb_record_bridge ||
         des->empty.record_type == sdb_record_msi) && 
        des->device.sdb_component.product.vendor_id == record->vendor_id &&
        des->device.sdb_component.product.device_id == record->device_id) {
      if (record->fill_output < record->size_output) 
        memcpy(record->output+record->fill_output, des, sizeof(struct sdb_device));
      ++record->fill_output;
      if (record->fill_msi < record->size_msi) {
        record->output_first[record->fill_msi] = msi_first;
        record->output_last [record->fill_msi] = msi_last;
      }
      ++record->fill_msi;
    }
  }
}

static eb_status_t eb_sdb_find_by_identity_real(
  eb_device_t device,
  uint64_t vendor_id,
  uint32_t device_id,
  const struct sdb_bridge* bridge,
  eb_address_t msi_first,
  eb_address_t msi_last,
  struct sdb_device* output,
  eb_address_t* output_msi_first,
  eb_address_t* output_msi_last,
  int* devices,
  int msis) 
{
  struct eb_find_by_identity record;
  
  record.vendor_id = vendor_id;
  record.device_id = device_id;
  record.size_output = *devices;
  record.size_msi = msis;
  record.fill_output = 0;
  record.fill_msi    = 0;
  record.pending = 1;
  record.output = output;
  record.output_first = output_msi_first;
  record.output_last  = output_msi_last;
  
  if (!bridge) {
    if ((record.status = eb_sdb_scan_root_msi(device, &record, eb_cb_find_by_identity)) == EB_OK)
      while (record.pending > 0) 
        eb_socket_run(eb_device_socket(device), -1);
  } else {
    if ((record.status = eb_sdb_scan_bus_msi(device, bridge, msi_first, msi_last, &record, eb_cb_find_by_identity)) == EB_OK)
      while (record.pending > 0) 
        eb_socket_run(eb_device_socket(device), -1);
  
  }
  
  *devices = record.fill_output;
  return record.status;
}

eb_status_t eb_sdb_find_by_identity_msi(eb_device_t device, uint64_t vendor_id, uint32_t device_id, struct sdb_device* output, eb_address_t* output_msi_first, eb_address_t* output_msi_last, int* devices) {
  return eb_sdb_find_by_identity_real(device, vendor_id, device_id, 0, 0, 0, output, output_msi_first, output_msi_last, devices, *devices);
}

eb_status_t eb_sdb_find_by_identity(eb_device_t device, uint64_t vendor_id, uint32_t device_id, struct sdb_device* output, int* devices) {
  return eb_sdb_find_by_identity_real(device, vendor_id, device_id, 0, 0, 0, output, 0, 0, devices, 0);
}

eb_status_t eb_sdb_find_by_identity_at_msi(eb_device_t device, const struct sdb_bridge* bridge, eb_address_t msi_first, eb_address_t msi_last, uint64_t vendor_id, uint32_t device_id, struct sdb_device* output, eb_address_t* output_msi_first, eb_address_t* output_msi_last, int* devices) {
  return eb_sdb_find_by_identity_real(device, vendor_id, device_id, bridge, msi_first, msi_last, output, output_msi_first, output_msi_last, devices, *devices);
}

eb_status_t eb_sdb_find_by_identity_at(eb_device_t device, const struct sdb_bridge* bridge, uint64_t vendor_id, uint32_t device_id, struct sdb_device* output, int* devices) {
  return eb_sdb_find_by_identity_real(device, vendor_id, device_id, bridge, 0, 0, output, 0, 0, devices, 0);
}
