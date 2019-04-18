#include <stdio.h>

#include <etherbone.h>


#define MSI_MAILBOX_VENDOR        0x651
#define MSI_MAILBOX_PRODUCT       0xfab0bdd8

eb_status_t read_handler(eb_user_data_t user, eb_address_t adr , eb_width_t width, eb_data_t* data)
{
	printf("read_handler called: adr=0x%08X, data=0x%08X\n", adr, data);
	return EB_OK;
}
eb_status_t write_handler(eb_user_data_t user, eb_address_t adr, eb_width_t width, eb_data_t data)
{
	printf("write_handler called: adr=0x%08X, data=0x%08X\n", adr, data);
	return EB_OK;
}

int main(int argc, char *argv[])
{

	if (argc == 1) {
		printf("usage: %s <port>\n", argv[0]);
		return 0;
	}

	eb_socket_t eb_socket;
	if (eb_socket_open(EB_ABI_CODE, 0, EB_DATAX|EB_ADDRX, &eb_socket) != EB_OK) return 1;

	eb_device_t eb_device;
	if (eb_device_open(eb_socket, argv[1], EB_ADDRX|EB_DATAX, 5, &eb_device) != EB_OK) return 2;
  printf("%s: Opened device 0x%08X\n", __FUNCTION__, eb_device);

	eb_address_t first, last;
	if (eb_device_enable_msi(eb_device, &first, &last) != EB_OK) return 3;
  printf("%s: Enabled MSI first=0x%08X last= 0x%08X\n", __FUNCTION__, first, last);

	// find the MBOX device on the TOP crossbar
	int size = 1;
	struct sdb_device mbox_dev;
	if (eb_sdb_find_by_identity(eb_device, MSI_MAILBOX_VENDOR, MSI_MAILBOX_PRODUCT, &mbox_dev, &size) != EB_OK) return 4;
  printf("%s: Found the MBOX device on the TOP crossbar mbox_dev=0x%08X size=%d\n", __FUNCTION__, mbox_dev, size);

	// find the MBOX device on the TOP-MSI crossbar
	struct sdb_device mbox_msi;
	eb_address_t mbox_msi_first, mbox_msi_last;
	if (eb_sdb_find_by_identity_msi(eb_device, MSI_MAILBOX_VENDOR, MSI_MAILBOX_PRODUCT, &mbox_msi, &mbox_msi_first, &mbox_msi_last, &size) != EB_OK) return 5;
  printf("%s: Found the MBOX device on the TOP-MSI crossbar mbox_msi=0x%08X first=0x%08X last=0x%08X\n", __FUNCTION__, mbox_msi, mbox_msi_first, mbox_msi_last);



	// IRQ address that identifies our program
	int arbitrary_number_between_first_and_last = first + 0xaa;
	eb_address_t irq_adr = mbox_msi_first + arbitrary_number_between_first_and_last;


	// find free slot in Mailbox and configure it to target out program
	int num_mbox_slots = 128;
	eb_data_t mailbox_value;
	for(int slot = 0; slot < num_mbox_slots; ++slot) {
		// each mailbox slot consists of two 32bit registers. 
		// the second one is used to configure the MSI target address.
		// the first one is later used to fire an MSI interrupt.
		eb_address_t mailbox_slot_adr = mbox_dev.sdb_component.addr_first + slot*8 + 4;

		//printf("addr: 0x%08x\n",  mbox_dev.sdb_component.addr_first + slot*8 + 4); // one mbox slot is 8 bytes wide
		eb_device_read(eb_device, mailbox_slot_adr, EB_DATA32, &mailbox_value, 0, 0);
		if (mailbox_value == 0xffffffff) {
			// configure the first free (equal to 0xfffffff) mailbox slot with our programs' irq_adr
			eb_device_write(eb_device, mailbox_slot_adr, EB_DATA32, (eb_data_t)irq_adr, 0, 0);
			printf("%s: mailbox slot %d configured with address 0x%08X\nMSI can be triggered by writing a value to eb-address 0x%08X\n", 
             __FUNCTION__, slot, irq_adr, mailbox_slot_adr - 4);
			break;
		}
		if (slot == num_mbox_slots-1) {
			printf("%s: No free mailbox slot\n", __FUNCTION__);
			return 5;
		}
	}

	// Prepare an IRQ handler.
	// This involves creating an SDB entry for our program 
	// so that etherbone can treat it as MSI target.
	struct sdb_device everything = {
		.abi_class                       = 0,
		.abi_ver_major                   = 0,
		.abi_ver_minor                   = 0,
		.bus_specific                    = SDB_WISHBONE_WIDTH,
		.sdb_component.addr_first        = 0,
		.sdb_component.addr_last         = UINT32_C(0xffffffff),
		.sdb_component.product.vendor_id = 0x651,
		.sdb_component.product.device_id = 0xefaa70,
		.sdb_component.product.version   = 1,
		.sdb_component.product.date      = 0x20150225,
		.sdb_component.product.name      = "SAFTLIB           " // char* constant must have length of 19
	};

	struct eb_handler irq_handler = {
		.device = &everything,
		.read  = &read_handler,
		.write = &write_handler,
	};

	eb_socket_attach(eb_socket, &irq_handler);

	int timeout_us = 5000000; // 2 second
	while (1) {
		// Wait for incoming MSIs on the socket, with a 1s timeout
		// Interrupts can be triggered by anybody who can write to the configured Mailbox slot address.
		// This includes the host using eb-write as well as LM32 inside the hardware.
    printf("\n\n\n\n\n%s: ==============================================================\n", __FUNCTION__);
    printf("%s: Running socket\n", __FUNCTION__);
		eb_socket_run(eb_socket, timeout_us);
		printf("%s: Timeout\n", __FUNCTION__);
	}

	// This program does not do any cleanup. Normally, on program shutdown, the Mailbox slot should be freed 
	// by writing 0xffffffff into the slot configuration register.

	return 0;
}
