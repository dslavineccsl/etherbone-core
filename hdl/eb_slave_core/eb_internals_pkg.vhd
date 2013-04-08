library IEEE;
--! Standard packages
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

--! Additional library
library work;
use work.wishbone_pkg.all;


package eb_internals_pkg is 

  subtype t_tag is std_logic_vector(1 downto 0);

  constant c_tag_wbm_req : t_tag := "00";
  constant c_tag_cfg_req : t_tag := "01";
  constant c_tag_pass_on : t_tag := "10";

  component eb_slave is
    generic(
      g_sdb_address : t_wishbone_address);
    port(
      clk_i       : in std_logic;  --! System Clk
      nRst_i      : in std_logic;  --! active low sync reset

      EB_RX_i     : in  t_wishbone_slave_in;   --! Streaming wishbone(record) sink from RX transport protocol block
      EB_RX_o     : out t_wishbone_slave_out;  --! Streaming WB sink flow control to RX transport protocol block
      EB_TX_i     : in  t_wishbone_master_in;  --! Streaming WB src flow control from TX transport protocol block
      EB_TX_o     : out t_wishbone_master_out; --! Streaming WB src to TX transport protocol block

      WB_config_i : in  t_wishbone_slave_in;    --! WB V4 interface to WB interconnect/device(s)
      WB_config_o : out t_wishbone_slave_out;   --! WB V4 interface to WB interconnect/device(s)
      WB_master_i : in  t_wishbone_master_in;   --! WB V4 interface to WB interconnect/device(s)
      WB_master_o : out t_wishbone_master_out;  --! WB V4 interface to WB interconnect/device(s)
      
      my_mac_o    : out std_logic_vector(47 downto 0);
      my_ip_o     : out std_logic_vector(31 downto 0);
      my_port_o   : out std_logic_vector(15 downto 0));
  end component;

  component eb_rx_fsm is
    port (
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      rx_cyc_i    : in  std_logic;
      rx_stb_i    : in  std_logic;
      rx_dat_i    : in  std_logic_vector(31 downto 0);
      rx_stall_o  : out std_logic;
      tx_cyc_o    : out std_logic;
      
      mux_empty_i : in  std_logic;
      
      tag_stb_o   : out std_logic;
      tag_dat_o   : out t_tag;
      tag_full_i  : in  std_logic;
      
      pass_stb_o  : out std_logic;
      pass_dat_o  : out std_logic_vector(31 downto 0); 
      pass_full_i : in  std_logic;
      
      cfg_wb_o    : out t_wishbone_master_out;  -- cyc always hi
      cfg_full_i  : in  std_logic;
      
      wbm_wb_o    : out t_wishbone_master_out;
      wbm_full_i  : in  std_logic );
  end component;

  component eb_fifo is
    generic(
      g_width : natural;
      g_size  : natural);
    port(
      clk_i     : in  std_logic;
      rstn_i    : in  std_logic;
      w_full_o  : out std_logic;
      w_push_i  : in  std_logic;
      w_dat_i   : in  std_logic_vector(g_width-1 downto 0);
      r_empty_o : out std_logic;
      r_pop_i   : in  std_logic;
      r_dat_o   : out std_logic_vector(g_width-1 downto 0));
  end component;

  component eb_tx_mux is
    port(
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      tag_pop_o   : out std_logic;
      tag_dat_i   : in  t_tag;
      tag_empty_i : in  std_logic;
      
      pass_pop_o   : out std_logic;
      pass_dat_i   : in  std_logic_vector(31 downto 0); 
      pass_empty_i : in  std_logic;
      
      cfg_pop_o    : out std_logic;
      cfg_dat_i    : in  std_logic_vector(31 downto 0);
      cfg_empty_i  : in  std_logic;
      
      wbm_pop_o    : out std_logic;
      wbm_dat_i    : in  std_logic_vector(31 downto 0);
      wbm_empty_i  : in  std_logic;
      
      tx_stb_o     : out std_logic;
      tx_dat_o     : out std_logic_vector(31 downto 0);
      tx_stall_i   : in  std_logic);
  end component;

  component eb_tag_fifo is
    port(
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      fsm_stb_i   : in  std_logic;
      fsm_dat_i   : in  t_tag;
      fsm_full_o  : out std_logic;

      mux_pop_i   : in  std_logic;
      mux_dat_o   : out t_tag;
      mux_empty_o : out std_logic);
  end component;

  component eb_pass_fifo is
    port(
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      fsm_stb_i   : in  std_logic;
      fsm_dat_i   : in  std_logic_vector(31 downto 0);
      fsm_full_o  : out std_logic;

      mux_pop_i   : in  std_logic;
      mux_dat_o   : out std_logic_vector(31 downto 0);
      mux_empty_o : out std_logic);
  end component;

  component eb_cfg_fifo is
    generic(
      g_sdb_address : t_wishbone_address);
    port(
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      errreg_i    : in  std_logic_vector(63 downto 0);
      
      cfg_i       : in  t_wishbone_slave_in;
      cfg_o       : out t_wishbone_slave_out;
      
      fsm_wb_i    : in  t_wishbone_master_out;
      fsm_full_o  : out std_logic;

      mux_pop_i   : in  std_logic;
      mux_dat_o   : out std_logic_vector(31 downto 0);
      mux_empty_o : out std_logic;
      
      my_mac_o    : out std_logic_vector(47 downto 0);
      my_ip_o     : out std_logic_vector(31 downto 0);
      my_port_o   : out std_logic_vector(15 downto 0));
  end component;

  component eb_wbm_fifo is
    port(
      clk_i       : in  std_logic;
      rstn_i      : in  std_logic;
      
      errreg_o    : out std_logic_vector(63 downto 0);
      busy_o      : out std_logic;
      
      wb_stb_o    : out std_logic;
      wb_adr_o    : out t_wishbone_address;
      wb_sel_o    : out t_wishbone_byte_select;
      wb_we_o     : out std_logic;
      wb_dat_o    : out t_wishbone_data;
      wb_i        : in  t_wishbone_master_in;
      
      fsm_wb_i    : in  t_wishbone_master_out;
      fsm_full_o  : out std_logic;

      mux_pop_i   : in  std_logic;
      mux_dat_o   : out std_logic_vector(31 downto 0);
      mux_empty_o : out std_logic);
  end component;

end package;