library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;
use work.wr_fabric_pkg.all;

package eb_pcie_slave_pkg is


component eb_pcie_slave is
  generic(
    g_fast_ack          : boolean := true;
    g_sdb_address       : t_wishbone_address;
    g_timeout_cycles    : natural;
    g_fifo_size         : natural := 2048
  );
  port(
    clk_wb_i      : in  std_logic; -- clock from PCI WB side
    rstn_wb_i     : in  std_logic;    
    
    clk_xwb_i     : in  std_logic; -- clock from XWB side
    rstn_xwb_i    : in  std_logic;

    -- Command from PC to EB slave
    slave_i       : in  t_wishbone_slave_in := cc_dummy_slave_in;
    slave_o       : out t_wishbone_slave_out;
    
    -- Commands from EB slave to XWB crossbar
    master_o      : out t_wishbone_master_out;
    master_i      : in  t_wishbone_master_in;
    
    phy_cyc_o     : out std_logic
    );
end component;
   
end eb_pcie_slave_pkg;
