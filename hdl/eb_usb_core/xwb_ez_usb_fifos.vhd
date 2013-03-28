------------------------------------------------------------------------------
-- Title      : EZ-USB Slave FIFO bridge
-- Project    : General Cores Collection (gencores) library
------------------------------------------------------------------------------
-- File       : xwb_ez_usb_fifos.vhd
-- Author     : Wesley W. Terpstra
-- Company    : GSI
-- Created    : 2013-03-26
-- Last update: 2013-03-26
-- Platform   : FPGA-generic
-- Standard   : VHDL'93
-------------------------------------------------------------------------------
-- Description: A simple Wishbone mux that drives the off-chip EZ-USB FIFOs
--  This module
-------------------------------------------------------------------------------
-- Copyright (c) 2010 CERN
-------------------------------------------------------------------------------
-- Revisions  :
-- Date        Version  Author          Description
-- 2010-05-18  1.0      terpstra        Created
-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;

entity xwb_ez_usb_fifos is
  generic(
    g_clock_period : integer := 16; -- clk_sys_i in ns
    g_board_delay  : integer := 2;  -- path length from FPGA to chip
    g_margin       : integer := 4;  -- too lazy to consider FPGA timing constraints? increase this.
    g_fifo_width   : integer := 8;  -- # of FIFO data pins connected (8 or 16)
    g_num_fifos    : integer := 4   -- always 4 for FX2LP (EP2, EP4, EP6, EP8)
    );

  port(
    clk_sys_i : in  std_logic;
    rstn_i    : in  std_logic;

    -- Wishbone interface
    slave_i   : in  t_wishbone_slave_in_array (g_num_fifos-1 downto 0) := (others => cc_dummy_slave_in);
    slave_o   : out t_wishbone_slave_out_array(g_num_fifos-1 downto 0);

    -- External signals
    fifoadr_o : out std_logic_vector(f_ceil_log2(g_num_fifos)-1 downto 0) := (others => '0');
    flagbn_i  : in  std_logic := '0'; -- fifo full
    flagcn_i  : in  std_logic := '0'; -- fifo empty
    sloen_o   : out std_logic := '1'; -- output enable
    slrdn_o   : out std_logic := '1'; -- read enable
    slwrn_o   : out std_logic := '1'; -- write enable
    pktendn_o : out std_logic := '1'; -- packet end
    fd_i      : in  std_logic_vector(g_fifo_width-1 downto 0) := (others => '0');
    fd_o      : out std_logic_vector(g_fifo_width-1 downto 0) := (others => '0');
    fd_oen_o  : out std_logic := '0');

end xwb_ez_usb_fifos;

architecture rtl of xwb_ez_usb_fifos is
  -- Timing constants from EZ-USB data sheet
  constant c_tXFLG_a : integer :=  11; -- FIFOADR to FLAGS output propagation delay
  constant c_tXFLG_r : integer :=  70; -- SLRD to FLAGS output propagation delay
  constant c_tXFLG_w : integer :=  70; -- SLWR to FLAGS output propagation delay
  constant c_tXFLG_e : integer := 115; -- PKTEND to FLAGS output propagation delay
  constant c_tSFA    : integer :=  10; -- FIFOADR to SLRD/SLWR/PKTEND setup time (adr -> falling edge)
  constant c_tFAH    : integer :=  10; -- RD/WR/PKTEND to FIFOADR hold time (rising edge -> adr)
  constant c_tRDpwl  : integer :=  50; -- SLRD pulse width low (active)
  constant c_tRDpwh  : integer :=  50; -- SLRD pulse width high
  constant c_tXFD    : integer :=  15; -- SLRD to FIFO data output propagation delay
  constant c_tOEon   : integer :=  11; -- SLOE turn-on (low) to FIFO data valid
  constant c_tOEoff  : integer :=  11; -- SLOE turn-off to FIFO data hold
  constant c_tWRpwl  : integer :=  50; -- SLWR pulse width low (active)
  constant c_tWRpwh  : integer :=  70; -- SLWR pulse width high
  constant c_tSFD    : integer :=  10; -- SLWR to FIFO DATA setup time (data must be valid before rising edge)
  constant c_tFDH    : integer :=  10; -- FIFO DATA to SLWR hold time
  constant c_tPEpwl  : integer :=  50; -- PKTEND pulse width low
  constant c_tPEpwh  : integer :=  50; -- PKTEND pulse width high
  
  type t_state is (LATCH_FLAGS,  DISPATCH,   SET_ADDR,
                   DRIVE_READ,   LATCH_DATA, IDLE_READ,
                   DRIVE_WRITE,  IDLE_WRITE, IDLE_DATA,
                   DRIVE_PKTEND, IDLE_PKTEND);
  
  function f_cycles(x : integer) return integer is
  begin
    if x+g_margin <= 0 then return 1; else return (x+g_margin+g_clock_period-1)/g_clock_period; end if;
  end f_cycles;
  function f_ns(x : integer) return integer is
  begin
    return x*g_clock_period;
  end f_ns;
  
  function f_max(x, y : integer) return integer is
  begin
    if x > y then return x; else return y; end if;
  end f_max;
  
  -- Derive the number of cycles we stay in each state
  constant c_latch_flags  : integer := 2; -- >= 2 to synchronize async signal
  constant c_dispatch     : integer := 1;
  constant c_set_addr     : integer := f_cycles(f_max(c_tXFLG_a + g_board_delay*2,
                                                      c_tSFA - f_ns(c_latch_flags+c_dispatch)));
  constant c_drive_read   : integer := f_cycles(f_max(c_tOEon, c_tXFD) + g_board_delay*2);
  constant c_latch_data   : integer := f_cycles(c_tRDpwl - f_ns(c_drive_read));
  constant c_idle_read    : integer := f_cycles(f_max(c_tRDpwh   - f_ns(c_latch_flags+c_dispatch),
                                                f_max(c_tXFLG_r - f_ns(c_drive_read+c_latch_data),
                                                f_max(c_tOEoff  - f_ns(c_latch_data),
                                                      c_tFAH))));
  constant c_drive_write  : integer := f_cycles(f_max(c_tWRpwl, c_tSFD));
  constant c_idle_write   : integer := f_cycles(c_tFDH);
  constant c_idle_data    : integer := f_cycles(f_max(c_tWRpwh  - f_ns(c_latch_flags+c_dispatch+c_idle_write),
                                                f_max(c_tXFLG_w - f_ns(c_drive_write+c_idle_write),
                                                      c_tFAH    - f_ns(c_idle_write))));
  constant c_drive_pktend : integer := f_cycles(c_tPEpwl);
  constant c_idle_pktend  : integer := f_cycles(f_max(c_tPEpwh  - f_ns(c_latch_flags+c_dispatch),
                                                f_max(c_tXFLG_e - f_ns(c_drive_pktend),
                                                      c_tFAH)));
  
  constant c_max_count : integer := 
    f_max(c_latch_flags, f_max(c_dispatch,  f_max(c_set_addr,    f_max(c_drive_read, 
    f_max(c_latch_data,  f_max(c_idle_read, f_max(c_drive_write, f_max(c_idle_write, 
    f_max(c_idle_data,   f_max(c_drive_pktend, c_idle_pktend))))))))));
  
  
  subtype word is std_logic_vector(g_fifo_width-1 downto 0);
  type words is array(natural range <>) of word;
  
  signal state    : t_state                                       := LATCH_FLAGS;
  signal notempty : std_logic_vector(c_latch_flags-1 downto 0)    := (others => '0');
  signal notfull  : std_logic_vector(c_latch_flags-1 downto 0)    := (others => '0');
  signal count    : unsigned(f_ceil_log2(c_max_count)-1 downto 0) := (others => '0');
  signal addr     : unsigned(f_ceil_log2(g_num_fifos)-1 downto 0) := (others => '0');
  signal dat4usb  : words(g_num_fifos-1 downto 0)                 := (others => (others => '0'));
  signal dat4wb   : words(g_num_fifos-1 downto 0)                 := (others => (others => '0'));
  signal needend  : std_logic_vector(g_num_fifos-1 downto 0)      := (others => '0');
  signal request  : std_logic_vector(g_num_fifos-1 downto 0)      := (others => '0');
  signal write    : std_logic_vector(g_num_fifos-1 downto 0)      := (others => '0');
  signal stall    : std_logic_vector(g_num_fifos-1 downto 0)      := (others => '1');
  signal ack      : std_logic_vector(g_num_fifos-1 downto 0)      := (others => '0');
  
begin

  -- Drive unregistered WB signals
  wbs : for i in 0 to g_num_fifos-1 generate
    slave_o(i).ack <= ack(i);
    slave_o(i).err <= '0';
    slave_o(i).rty <= '0';
    slave_o(i).int <= '0';
    slave_o(i).stall <= stall(i);
    slave_o(i).dat(word'range) <= dat4wb(i);
    slave_o(i).dat(c_wishbone_data_width-1 downto g_fifo_width) <= (others => '0');
  end generate;

  fifoadr_o <= std_logic_vector(addr);

  fsm : process(clk_sys_i, rstn_i) is
  begin
    if rstn_i = '0' then
      state    <= LATCH_FLAGS;
      notempty <= (others => '0');
      notfull  <= (others => '0');
      count    <= (others => '0');
      addr     <= (others => '0');
      dat4usb  <= (others => (others => '0'));
      dat4wb   <= (others => (others => '0'));
      needend  <= (others => '0');
      request  <= (others => '0');
      write    <= (others => '0');
      stall    <= (others => '1');
      ack      <= (others => '0');
      
      sloen_o   <= '1';
      slrdn_o   <= '1';
      slwrn_o   <= '1';
      pktendn_o <= '1';
      
      fd_o      <= (others => '0');
      fd_oen_o  <= '0';
      
    elsif rising_edge(clk_sys_i) then
      case state is
        
        when LATCH_FLAGS =>
          notfull  <= flagbn_i & notfull(notfull'left downto 1);
          notempty <= flagcn_i & notempty(notempty'left downto 1);
          
          stall(to_integer(addr)) <= request(to_integer(addr));
          
          if stall  (to_integer(addr))     = '0' and
             slave_i(to_integer(addr)).cyc = '1' and
             slave_i(to_integer(addr)).stb = '1' then
            stall  (to_integer(addr)) <= '1';
            request(to_integer(addr)) <= '1';
            write  (to_integer(addr)) <= slave_i(to_integer(addr)).we;
            dat4usb(to_integer(addr)) <= slave_i(to_integer(addr)).dat(word'range);
          end if;
          
          if count /= c_latch_flags-1 then
            count <= count + 1;
          else
            stall(to_integer(addr)) <= '1';
            state <= DISPATCH;
            count <= (others => '0');
          end if;
        
        when DISPATCH =>
          state <= SET_ADDR; -- default
          count <= (others => '0');
          
          if request(to_integer(addr)) = '1' then
            if write(to_integer(addr)) = '1' then
              if notfull(0) = '1' then
                state <= DRIVE_WRITE;
                request(to_integer(addr)) <= '0';
              end if;
            else
              if notempty(0) = '1' then
                state <= DRIVE_READ;
                request(to_integer(addr)) <= '0';
              end if;
            end if;
          else
            if needend(to_integer(addr)) = '1' then
              state <= DRIVE_PKTEND;
            end if;
          end if;
          
        when SET_ADDR =>
          if count = 0 then
            if addr = g_num_fifos-1 then
              addr <= (others => '0');
            else
              addr <= addr + 1;
            end if;
          end if;
          
          if count /= c_set_addr-1 then
            count <= count + 1;
          else
            state <= LATCH_FLAGS;
            count <= (others => '0');
          end if;
          
        when DRIVE_READ =>
          sloen_o <= '0';
          slrdn_o <= '0';
          
          if count /= c_drive_read-1 then
            count <= count + 1;
          else
            state <= LATCH_DATA;
            count <= (others => '0');
          end if;
          
        when LATCH_DATA =>
          sloen_o <= '1';
          
          if count = 0 then
            dat4wb(to_integer(addr)) <= fd_i;
          end if;
          
          if count /= c_latch_data-1 then
            count <= count + 1;
          else
            state <= IDLE_READ;
            count <= (others => '0');
            ack(to_integer(addr)) <= '1';
          end if;
          
        when IDLE_READ =>
          slrdn_o <= '1';
          ack(to_integer(addr)) <= '0';
          
          if count /= c_idle_read-1 then
            count <= count + 1;
          else
            state <= LATCH_FLAGS;
            count <= (others => '0');
          end if;
          
        when DRIVE_WRITE =>
          slwrn_o  <= '0';
          fd_oen_o <= '1';
          fd_o     <= dat4usb(to_integer(addr));
          needend(to_integer(addr)) <= '1';
          
          if count /= c_drive_write-1 then
            count <= count + 1;
          else
            state <= IDLE_WRITE;
            count <= (others => '0');
            ack(to_integer(addr)) <= '1';
          end if;
          
        when IDLE_WRITE =>
          slwrn_o <= '1';
          ack(to_integer(addr)) <= '0';
          
          if count /= c_idle_write-1 then
            count <= count + 1;
          else
            state <= IDLE_DATA;
            count <= (others => '0');
          end if;
          
        when IDLE_DATA =>
          fd_oen_o <= '0';
          
          if count /= c_idle_data-1 then
            count <= count + 1;
          else
            state <= LATCH_FLAGS;
            count <= (others => '0');
          end if;
          
        when DRIVE_PKTEND =>
          pktendn_o <= '0';
          needend(to_integer(addr)) <= '0';
          
          if count /= c_drive_pktend-1 then
            count <= count + 1;
          else
            state <= IDLE_PKTEND;
            count <= (others => '0');
          end if;
          
        when IDLE_PKTEND =>
          pktendn_o <= '1';
          
          if count /= c_idle_pktend-1 then
            count <= count + 1;
          else
            state <= LATCH_FLAGS;
            count <= (others => '0');
          end if;

      end case;
    end if;
  end process;
end rtl;
