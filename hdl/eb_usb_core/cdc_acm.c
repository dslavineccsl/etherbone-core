/* -*- mode: C; c-basic-offset: 2; -*-
 *
 * cdc_acm -- FX2 USB serial port converter
 * 
 * by  Brent Baccala <cosine@freesoft.org>   July 2009
 * and Wolfgang Wieser ] wwieser (a) gmx <*> de [   Aug. 2009
 * and Wesley W. Terpstra  March 2013
 *
 * Based on both usb-fx2-local-examples by Wolfgang Wieser (GPLv2) and
 * the Cypress FX2 examples.
 *
 * This is an FX2 program which re-enumerates to a CDC-ACM device.
 * Anything transmitted to it by the host is forwarded to an FPGA.
 *
 * Features like setting and querying things like the baudrate and
 * line/modem status are unimplemented, and omitted in the descriptor.
 *
 * The program ignores USB Suspend interrupts, and probably violates
 * the USB standard in this regard, as all USB devices are required to
 * support Suspend.  The remote wakeup feature is parsed and correctly
 * handled in the protocol, but of course does nothing since the
 * device never suspends.
 */

#define ALLOCATE_EXTERN
#define xdata __xdata
#define at __at
#define sfr __sfr
#define bit __bit
#define code __code
#define sbit __sbit

#include "fx2regs.h"
#include "fx2.h"

// If Rwuen_allowed is TRUE, the device allows the remote wakeup
// feature to be set by the host.  If the host sets the feature, Rwuen
// becomes TRUE but nothing else is done.  If Rwuen_allowed is FALSE,
// the device will signal an error on an attempt to set remote wakeup.

BOOL Rwuen_allowed = FALSE;	// Allow remote wakeup to be enabled
BOOL Rwuen = FALSE;		// Remote wakeup enable
BOOL Selfpwr = FALSE;		// Device is (not) self-powered

// USB allows a device to support multiple configurations, only one of
// which can be active at any given time.  Each configuration can have
// multiple interfaces, all of which can be active simultaneously, and
// each interface can also have a setting.  This driver only uses a
// single configuration with a single interface, which has a single
// setting, but these variables are here as place holders in case you
// want to make the driver more sophisticated.

#define NUM_INTERFACES 3

BYTE Configuration;                         // Current device configuration
BYTE InterfaceSetting[NUM_INTERFACES];      // Current interface settings

// this table is used by the epcs macro 
const char code  EPCS_Offset_Lookup_Table[] =
{
   0,    // EP1OUT
   1,    // EP1IN
   2,    // EP2OUT
   2,    // EP2IN
   3,    // EP4OUT
   3,    // EP4IN
   4,    // EP6OUT
   4,    // EP6IN
   5,    // EP8OUT
   5,    // EP8IN
};

// macro for generating the address of an endpoint's control and status register (EPnCS)
#define epcs(EP) (*(BYTE xdata*)(EPCS_Offset_Lookup_Table[(EP & 0x7E) | (EP > 128)] + 0xE6A1))

#define DEVICE_LEN 18
#define DEVQUAL_LEN 10
#define CONFIG_LEN 9
#define INTRFC_LEN 9
#define ENDPNT_LEN 7
#define HEADER_LEN 5
#define CALLM_LEN  5
#define ACM_FM_LEN 4
#define UNION_LEN  5

#define CS_DSCR 0x24
#define HEADER_DSCR 0
#define CALLM_DSCR  1
#define ACM_FM_DSCR 2
#define UNION_DSCR  6


// USB string descriptors are constructed from this array.  Indices
// into this array can be placed into the other descriptors to specify
// manufacturer name, serial number, etc.  Index 0 is reserved for a
// list of language codes that this program currently ignores, but in
// principle you can switch between different string tables based on
// the language requested by the host.
//
// The wire format is specified in 16-bit Unicode.  I just use ASCII
// here and add zero bytes when I construct the string descriptor.  If
// you actually want to specify Unicode here, disable the code that
// doubles up these bytes with zeros.
//
// Also, since I build the string descriptors directly in the EP0
// output buffer, I'm limited to 31 character strings (64 byte packet;
// 2 char overhead; 16 bits per char).  If this is a problem, I'd
// suggest allocating a block of memory for a longer string descriptor
// (just like the other descriptors below), building the string
// descriptors there, then transferring them using the same SUDPTR
// technique used for the other descriptors, which can handle larger
// descriptors and breaks them up automatically into small packets.

const char * xdata USB_strings[] = { 
  "EN", 
  "GSI Helmholtzzentrum", 
  "USB-{Serial,Wishbone} adapter", 
  "White Rabbit Console (Comm)",
  "White Rabbit Console (Data)",
  "USB-Wishbone Bridge"
};

// Descriptors are a real pain since Device and Configuration
// descriptors have to be word-aligned (this is an FX2 requirement),
// but Interface and Endpoint descriptors can't be word aligned - they
// have directly follow their corresponding Configuration descriptor.

unsigned char xdata myDeviceDscr[] = {
   DEVICE_LEN,			// Descriptor length
   DEVICE_DSCR,			// Descriptor type
   0, 2,			// USB spec version 2.00
   2,				// Device class (CDC)
   0,				// Device sub-class
   0,				// Device sub-sub-class
   64,				// Max packet size for EP0 (bytes)
   0x50, 0x1d,			// Vendor ID  (openmoko      = 1d50)
   0x62, 0x60,			// Product ID (USB-WB bridge = 6062)
   0, 1,			// Product version (1.00)
   1,				// Manufacturer string index
   2,				// Product string index
   0,				// Serial number string index
   1				// Number of configurations
};

// In theory, this descriptor returns information about the "other"
// speed.  So, if we're operating at high speed, the Device descriptor
// would contain high speed configuration information and this
// descriptor would contain full speed information, and vice versa if
// we're operating at full speed.  I take a simpler approach - the
// configuration information is the same for both speeds, so we never
// care which one is which.

unsigned char xdata myDeviceQualDscr[] = {
   DEVQUAL_LEN,			// Descriptor length
   DEVQUAL_DSCR,		// Descriptor type
   0, 2,			// USB spec version 2.00
   2,				// Device class
   0,				// Device sub-class
   0,				// Device sub-sub-class
   64,				// Max packet size for EP0 (bytes)
   1,				// Number of alternate configurations
   0                            // reserved
};

// Likewise, we should another full set of these configuration
// descriptors.  In this case, however, the structures are identical
// and the few fields that differ (descriptor type and endpoint max
// packet size) can be tweaked at run-time, so this next set of
// descriptors does double duty as both Configuration and Other Speed
// Configuration descriptors.

unsigned char xdata at 0x3d00 myConfigDscr[] = {
   CONFIG_LEN,			// Descriptor length
   CONFIG_DSCR,			// Descriptor Type
   LSB(sizeof(myConfigDscr)),	// Total len of (config + intrfcs + end points)
   MSB(sizeof(myConfigDscr)),
   NUM_INTERFACES,		// Number of interfaces supported
   1,				// Configuration index for SetConfiguration()
   0,				// Config descriptor string index
   bmBUSPWR|bmRWU,		// Attributes
   0x1E,			// Max power consumption; 2*30 = 60 mA
   
   INTRFC_LEN,			// Descriptor length
   INTRFC_DSCR,			// Descriptor type
   0,				// Index of this interface (zero based)
   0,				// Value used to select alternate
   1,				// Number of endpoints
   0x02,			// Class Code    (CDC Comm)
   0x02,			// Subclass Code (ACM)
   0x00,			// Protocol Code (none)
   3,				// Index of interface string description
   
   HEADER_LEN,			// Descriptor length
   CS_DSCR,			// Descriptor type
   HEADER_DSCR,			// Descriptor subtype
   0x10, 0x01,			// 1.10 standard
   
   CALLM_LEN,			// Descriptor length
   CS_DSCR,			// Descriptor type
   CALLM_DSCR,			// Descriptor subtype
   0x0,				// bmCapabilities (no call management)
   0x1,				// bDataInterface
   
   ACM_FM_LEN,			// Descriptor length
   CS_DSCR,			// Descriptor type
   ACM_FM_DSCR,			// Descriptor subtype
   0x0,				// bmCapabilities (no extra SETUP_CLASS_REQUESTs supported)
   
   UNION_LEN,			// Descriptor length
   CS_DSCR,			// Descriptor type
   UNION_DSCR,			// Descriptor subtype
   0,				// bMasterInterface
   1,				// bSlaveInterface0
   
   ENDPNT_LEN,			// Descriptor length
   ENDPNT_DSCR,			// Descriptor type
   0x81,			// Endpoint #1 is IN
   EP_INT,			// Endpoint type (interrupt)
   64, 0,			// Maximum packet size
   0x02,			// Polling interval
   
   INTRFC_LEN,			// Descriptor length
   INTRFC_DSCR,			// Descriptor type
   1,				// Index of interface
   0,				// Alternate config
   2,				// Number of endpoints
   0x0A,			// Class code (CDC data)
   0x0,				// Subclass code
   0x0,				// Protocol code
   4,				// Interface descriptor string index
   
   ENDPNT_LEN,			// Descriptor length
   ENDPNT_DSCR,			// Descriptor type
   0x88,			// Endpoint #8 is IN
   EP_BULK,			// Endpoint type
   0, 2,			// Maximum packet size (512)
   0,				// Polling interval
   
   ENDPNT_LEN,			// Descriptor length
   ENDPNT_DSCR,			// Descriptor type
   0x04,			// Endpoint #4 is OUT
   EP_BULK,			// Endpoint type
   0, 2,			// Maximum packet size (512)
   0,				// Polling interval
   
   INTRFC_LEN,			// Descriptor length
   INTRFC_DSCR,			// Descriptor type
   2,				// Index of interface
   0,				// Alternate config
   2,				// Number of endpoints
   0xFF,			// Class code (vendor specific)
   0xFF,			// Subclass code
   0xFF,			// Protocol code
   5,				// Interface descriptor string index
   
   ENDPNT_LEN,			// Descriptor length
   ENDPNT_DSCR,			// Descriptor type
   0x86,			// Endpoint #6 is IN
   EP_BULK,			// Endpoint type
   0, 2,			// Maximum packet size (512)
   0,				// Polling interval
   
   ENDPNT_LEN,			// Descriptor length
   ENDPNT_DSCR,			// Descriptor type
   0x02,			// Endpoint #2 is OUT
   EP_BULK,			// Endpoint type
   0, 2,			// Maximum packet size (512)
   0				// Polling interval
};

// Read TRM 15.14 for an explanation of which commands need this.
// We use a few extra NOPs to make sure the timing works out.
#define	NOP __asm nop __endasm
static void syncdelay(void) {
  NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP;
  NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP;
  NOP;
}

//-----------------------------------------------------------------------------
// Endpoint 0 Device Request handler
//-----------------------------------------------------------------------------

static void ModifyDscr (unsigned char cfg, unsigned int speed) {
  unsigned char* s = &myConfigDscr[0];
  unsigned char* e = s + sizeof(myConfigDscr);
  
  s[1] = cfg;
  for (; s != e; s += *s) {
    if (s[1] == ENDPNT_DSCR && s[3] == EP_BULK) {
      s[4] = LSB(speed);
      s[5] = MSB(speed);
    }
  }
}

static void SetupCommand (void) {
  int i;
  int interface;

  // Errors are signaled by stalling endpoint 0.

  switch (SETUPDAT[0] & SETUP_MASK) {

  case SETUP_STANDARD_REQUEST:
    switch (SETUPDAT[1]) {
    case SC_GET_DESCRIPTOR:
      switch (SETUPDAT[3]) {
      case GD_DEVICE:
        SUDPTRH = MSB (&myDeviceDscr);
        SUDPTRL = LSB (&myDeviceDscr);
        break;

      case GD_DEVICE_QUALIFIER:
        SUDPTRH = MSB (&myDeviceQualDscr);
        SUDPTRL = LSB (&myDeviceQualDscr);
        break;
        
      case GD_CONFIGURATION:
        if (USBCS & bmHSM) {
          // High speed parameters
          ModifyDscr(CONFIG_DSCR, 512);
        } else {
          // Full speed parameters
          ModifyDscr(CONFIG_DSCR, 64);
        }
        SUDPTRH = MSB (&myConfigDscr);
        SUDPTRL = LSB (&myConfigDscr);
        break;
        
      case GD_OTHER_SPEED_CONFIGURATION:
        if (USBCS & bmHSM) {
          // We're in high speed mode, this is the Other
          // descriptor, so these are full speed parameters
          ModifyDscr(OTHERSPEED_DSCR, 64);
        } else {
          // We're in full speed mode, this is the Other
          // descriptor, so these are high speed parameters
          ModifyDscr(OTHERSPEED_DSCR, 512);
        }
        SUDPTRH = MSB (&myConfigDscr);
        SUDPTRL = LSB (&myConfigDscr);
        break;
        
      case GD_STRING:
        if (SETUPDAT[2] >= sizeof (USB_strings) / sizeof (USB_strings[0])) {
          EZUSB_STALL_EP0 ();
        } else {
          for (i = 0; i < 31; i++) {
            if (USB_strings[SETUPDAT[2]][i] == '\0')
              break;
            EP0BUF[2*i + 2] = USB_strings[SETUPDAT[2]][i];
            EP0BUF[2*i + 3] = '\0';
          }
          EP0BUF[0] = 2*i + 2;
          EP0BUF[1] = STRING_DSCR; syncdelay();
          EP0BCH = 0;              syncdelay();
          EP0BCL = 2*i + 2;
        }
        break;
        
      default:                 // Invalid request
        EZUSB_STALL_EP0 ();
      }
      break;
      
    case SC_GET_INTERFACE:
      interface = SETUPDAT[4];
      if (interface < NUM_INTERFACES) {
        EP0BUF[0] = InterfaceSetting[interface];
        EP0BCH = 0;
        EP0BCL = 1;
      } else {
        EZUSB_STALL_EP0 ();
      }
      break;
    
    case SC_SET_INTERFACE:
      interface = SETUPDAT[4];
      if (interface < NUM_INTERFACES) {
        InterfaceSetting[interface] = SETUPDAT[2];
      } else {
        EZUSB_STALL_EP0 ();
      }
      break;
    
    case SC_SET_CONFIGURATION:
      Configuration = SETUPDAT[2];
      break;
    
    case SC_GET_CONFIGURATION:
      EP0BUF[0] = Configuration;
      EP0BCH = 0;
      EP0BCL = 1;
      break;
    
    case SC_GET_STATUS:
      switch (SETUPDAT[0]) {
      case GS_DEVICE:
        EP0BUF[0] = ((BYTE) Rwuen << 1) | (BYTE) Selfpwr;
        EP0BUF[1] = 0;
        EP0BCH = 0;
        EP0BCL = 2;
        break;
        
      case GS_INTERFACE:
        EP0BUF[0] = 0;
        EP0BUF[1] = 0;
        EP0BCH = 0;
        EP0BCL = 2;
        break;
        
      case GS_ENDPOINT:
        EP0BUF[0] = epcs (SETUPDAT[4]) & bmEPSTALL;
        EP0BUF[1] = 0;
        EP0BCH = 0;
        EP0BCL = 2;
        break;
        
      default:                 // Invalid Command
        EZUSB_STALL_EP0 ();
      }
      break;
    
    case SC_CLEAR_FEATURE:
      switch (SETUPDAT[0]) {
      case FT_DEVICE:
        if (SETUPDAT[2] == 1)
          Rwuen = FALSE;        // Disable Remote Wakeup
        else
          EZUSB_STALL_EP0 ();
        break;
        
      case FT_ENDPOINT:
        if (SETUPDAT[2] == 0) {
          epcs (SETUPDAT[4]) &= ~bmEPSTALL;
          EZUSB_RESET_DATA_TOGGLE (SETUPDAT[4]);
        }
        else
          EZUSB_STALL_EP0 ();
        break;
      }
      break;
      
    case SC_SET_FEATURE:
      switch (SETUPDAT[0]) {
      case FT_DEVICE:
        if ((SETUPDAT[2] == 1) && Rwuen_allowed)
          Rwuen = TRUE;         // Enable Remote Wakeup
        else
          EZUSB_STALL_EP0 ();
        break;
        
      case FT_ENDPOINT:
        epcs (SETUPDAT[4]) |= bmEPSTALL;
        break;
        
      default:
        EZUSB_STALL_EP0 ();
      }
      break;
      
    default:                   // *** Invalid Command
      EZUSB_STALL_EP0 ();
      break;
    }
    break;

  case SETUP_CLASS_REQUEST:
    switch (SETUPDAT[1]) {
    // required
    case 0x00: // SEND_ENCAPSULATED_COMMAND
    case 0x01: // GET_ENCAPSULATED_RESPONSE (triggered by notification ResposneAvailable)
      // wIndex  = le DAT[4..5] = iface
      // wLength = le DAT[6..7] = command
      // ... pretend we did it.
      break;
    
#if 0
    // bit1 of bmCapabilities + SERIAL_STATE notification
    case 0x20: // SET_LINE_CODING
    case 0x21: // GET_LINE_CODING
      // wIndex  = le DAT[4..5] = iface
      // wLength = le DAT[6..7] = 7
      
      // Peek in EP0BUF[0..6]: (write EP0BC[HL] to include answer)
      // 0-3 = rate in bits/second
      //   4 = stop bits: 0=>1, 1=>1.5, 2=>2
      //   5 = parity:    0=>none, 1=>odd, 2=>even, 3=>mark, 4=>space
      //   6 = data bits: 5,6,7,8,16
      // unsupported; fall through
      
    case 0x22: // SET_CONTROL_LINE_STATE
      // wValue = le DAT[2..3] = b0=DTR b1=RTS
      // wIndex = le DAT[4..5] = iface
      // unsupported; fall through
    
      
    // bit0 of bmCapabilities
    case 0x02: // SET_COMM_FEATURE
    case 0x03: // GET_COMM_FEATURE
    case 0x04: // CLEAR_COMM_FEATURE
      //  wValue = selector abstract_sate/country_setting
      // unsupported; fall through
    
    // bit2 of bmCapabilities
    case 0x23: // SEND_BREAK         (00100001b)
      // wValue = le DAT[2..3] = duration in milliseconds
      // wIndex = le DAT[4..5] = iface
      // unsupported; fall through
      
#endif
    default:
      EZUSB_STALL_EP0 (); // unsupported/invalid command for CDC
    }
    break;
  
  //case SETUP_VENDOR_REQUEST:
  // add cycle line control here
    
  default:
    EZUSB_STALL_EP0 ();
    break;
  }

  // Acknowledge handshake phase of device request
  EP0CS |= bmHSNAK;
}

static void USB_isr(void) __interrupt 8
{
   // Clear global USB IRQ
   EXIF &= ~0x10;

   // Clear SUDAV IRQ
   USBIRQ = 0x01;

   SetupCommand();
}

//-----------------------------------------------------------------------------
// Main program
//-----------------------------------------------------------------------------

static void Initialize(void) {
  syncdelay();
  CPUCS = 0x10;        syncdelay(); /* 48 MHz, output disabled */

  IFCONFIG     = 0xCB; syncdelay(); // Internal IFCLK, 48MHz; A,B as slave asynchronous FIFO
  REVCTL       = 0x03; syncdelay(); // See TRM...
  PINFLAGSAB   = 0x00; syncdelay(); // FLAGA = prog-level, FLAGB = full flag
  PINFLAGSCD   = 0x00; syncdelay(); // FLAGC = empty flag, FLAGD = unused
  PORTACFG     = 0x00; syncdelay(); // use PA7/FLAGD as GPIO
  FIFOPINPOLAR = 0x00; syncdelay(); // all FIFO pins active low
  
  EP2CFG       = 0x00; syncdelay(); // First, disable everything
  EP4CFG       = 0x00; syncdelay();
  EP6CFG       = 0x00; syncdelay();
  EP8CFG       = 0x00; syncdelay();
  EP2FIFOCFG   = 0x00; syncdelay();
  EP4FIFOCFG   = 0x00; syncdelay();
  EP6FIFOCFG   = 0x00; syncdelay();
  EP8FIFOCFG   = 0x00; syncdelay();
  OEA          = 0x00; syncdelay();

  EP1OUTCFG    = 0xa0; syncdelay(); // 1-10 ----  1=valid,  out,10=bulk,   64,   single
  EP1INCFG     = 0xb0; syncdelay(); // 1-11 ----  1=valid,  in, 11=int,    64,   single
  EP2CFG       = 0xa2; syncdelay(); // 1010 0-10  1=valid,0=out,10=bulk,0=512,10=double
  EP4CFG       = 0xa0; syncdelay(); // 1010 ----  1=valid,0=out,10=bulk,  512,   double
  EP6CFG       = 0xe2; syncdelay(); // 1110 0-10  1=valid,1=in, 10=bulk,0=512,10=double
  EP8CFG       = 0xe0; syncdelay(); // 1110 ----  1=valid,1=in, 10=bulk,  512,   double

  // To be sure, clear and reset all FIFOs although 
  // this is probably not strictly required. 
  FIFORESET = 0x80;  syncdelay();  // NAK all requests from host. 
  FIFORESET = 0x82;  syncdelay();  // Reset individual EP (2,4,6,8)
  FIFORESET = 0x84;  syncdelay();
  FIFORESET = 0x86;  syncdelay();
  FIFORESET = 0x88;  syncdelay();
  FIFORESET = 0x80;  syncdelay();  // NAK all requests from host. 
  FIFORESET = 0x00;  syncdelay();  // Resume normal operation. 

  // Prime the pump for output buffers
  OUTPKTEND = 0x82;  syncdelay();
  OUTPKTEND = 0x82;  syncdelay();
  OUTPKTEND = 0x82;  syncdelay();
  OUTPKTEND = 0x82;  syncdelay();
  OUTPKTEND = 0x84;  syncdelay();
  OUTPKTEND = 0x84;  syncdelay();
  OUTPKTEND = 0x84;  syncdelay();
  OUTPKTEND = 0x84;  syncdelay();

  //  bit7: 0
  //  bit6: INFM6  See TRM 15-29 (p.351): Signal line one clock earlier.
  //  bit5: OEP6
  //  bit4: AUTOOUT    1 = enable
  //  bit3: AUTOIN     1 = enable
  //  bit2: ZEROLENIN  0 = disable
  //  bit1: 0
  //  bit0: WORDWIDE   0 = 16bit (default)
  EP2FIFOCFG = 0x10; syncdelay();
  EP4FIFOCFG = 0x10; syncdelay();
  EP6FIFOCFG = 0x0C; syncdelay();
  EP8FIFOCFG = 0x0C; syncdelay();

  EP2AUTOINLENH = 0x02; syncdelay(); // auto commit 512-byte chunks
  EP2AUTOINLENL = 0x00; syncdelay();
  EP4AUTOINLENH = 0x02; syncdelay(); // auto commit 512-byte chunks
  EP4AUTOINLENL = 0x00; syncdelay();
  EP6AUTOINLENH = 0x02; syncdelay(); // auto commit 512-byte chunks
  EP6AUTOINLENL = 0x00; syncdelay();
  EP8AUTOINLENH = 0x02; syncdelay(); // auto commit 512-byte chunks
  EP8AUTOINLENL = 0x00; syncdelay();

  // Setup Data Pointer - AUTO mode
  //
  // In this mode, there are two ways to send data on EP0.  You
  // can either copy the data into EP0BUF and write the packet
  // length into EP0BCH/L to start the transfer, or you can
  // write the (word-aligned) address of a USB descriptor into
  // SUDPTRH/L; the length is computed automatically.
  SUDPTRCTL = 1;

  // Enable USB interrupt
  IE = 0x80;
  EIE = 0x01;

  // Enable SUDAV (setup data available) interrupt
  USBIE = 0x01;
}

void main(void) {
  // Disconnect the USB interface, initialize, renumerate, reconnect
  USBCS |= 0x08;
  Initialize();
  USBCS |= 0x02;
  USBCS &= ~(0x08);
  
  for (;;) { 
  }
}