library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
--use work.pcie_wb_pkg.all;
use work.wishbone_pkg.all;
use work.etherbone_pkg.all;
use work.eb_internals_pkg.all;
use work.eb_hdr_pkg.all;
use work.genram_pkg.all;

entity eb_pci_slave is
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
end eb_pci_slave;

architecture rtl of eb_pci_slave is

  constant c_CFG_REG_ARRAY_START    : std_logic_vector(3 downto 0) := x"0"; -- 0x00 -- 0x80
  constant c_RX_FIFO_DATA           : std_logic_vector(3 downto 0) := x"8"; -- 0x20 -- 0xA0
  constant c_TX_FIFO_DATA           : std_logic_vector(3 downto 0) := x"C"; -- 0x30 -- 0xB0
  
  constant c_CFG_REG_CYCLE      : std_logic_vector(2 downto 0) := "000"; -- 0x00 -- 0x80
  constant c_CFG_REG_STATUS     : std_logic_vector(2 downto 0) := "001"; -- 0x04 -- 0x84
  constant c_EB_CYCLE_MAX_TOUT  : std_logic_vector(2 downto 0) := "010"; -- 0x08 -- 0x88
  constant c_TX_DATA_COUNT      : std_logic_vector(2 downto 0) := "011"; -- 0x0C -- 0x8C
  constant c_CFG_REG_SCRATCH    : std_logic_vector(2 downto 0) := "111"; -- 0x1C -- 0x9C

  constant c_FIFO_COUNT_WIDHT : natural := f_log2_size(g_fifo_size);

  signal pci_tx_fifo_full   : std_logic;
  signal pci_tx_fifo_we     : std_logic;
  signal pci_tx_fifo_dati   : std_logic_vector(31 downto 0);
  
  signal pci_tx_fifo_empty  : std_logic;
  signal pci_tx_fifo_re     : std_logic;
  signal pci_tx_fifo_dato   : std_logic_vector(31 downto 0);
  signal tx_fifo_rd_count   : std_logic_vector(c_FIFO_COUNT_WIDHT-1 downto 0);

  signal pci_rx_fifo_full   : std_logic;
  signal pci_rx_fifo_we     : std_logic;
  signal pci_rx_fifo_dati   : std_logic_vector(31 downto 0);
  
  signal pci_rx_fifo_empty  : std_logic;
  signal pci_rx_fifo_re     : std_logic;
  signal pci_rx_fifo_dato   : std_logic_vector(31 downto 0);
  
  
  signal pci_rx_fifo_master_o : t_wishbone_master_out;
  signal pci_rx_fifo_master_i : t_wishbone_master_in;
  
  signal eb_rx_stream_master_o  : t_wishbone_master_out;
  signal eb_rx_stream_master_o2 : t_wishbone_master_out;  
  signal eb_rx_stream_master_i  : t_wishbone_master_in;
  
  signal eb_slave_int_master_o  : t_wishbone_master_out;
  signal eb_slave_int_master_o2 : t_wishbone_master_out;
  signal eb_slave_int_master_i  : t_wishbone_master_in;
  
  signal eb_tx_fifo_master_o    : t_wishbone_master_out;
  signal eb_tx_fifo_master_i    : t_wishbone_master_in;
  
  signal eb_rx_stream_master_cyc_sync : std_logic_vector(1 downto 0);
  
  signal r_cyc_tout_counter : unsigned(31 downto 0);
  signal r_eb_cyc_max_tout  : unsigned(31 downto 0) := x"FFFFFFF0";
  
  signal eb_cyc_tout        : std_logic;
  
  
  -- internal register to hold cycle line
  signal r_cyc              : std_logic;

  signal r_cyc_dly          : std_logic;
  signal r_ack_dly          : std_logic;
  signal r_cyc_tout_dis     : std_logic;
  
  signal tx_fifo_rst        : std_logic;
  signal r_tx_fifo_ack_dly  : std_logic;
  
  signal r_scratch  : std_logic_vector(31 downto 0);
  
begin


  slave_o.err   <= '0';                  
  slave_o.rty   <= '0';                  
  slave_o.stall <= pci_rx_fifo_full;     
  slave_o.int   <= not pci_tx_fifo_empty;


  -- internal register space
  p_cfg_reg: process(clk_wb_i)
  begin
    if rising_edge(clk_wb_i) then

        -- ack one clock after stb
        slave_o.ack <= slave_i.stb;
        
        -- control bar
        if slave_i.adr(5) = '0' then
            -- configuration registers reads
            case slave_i.adr(4 downto 2) is
                when c_CFG_REG_CYCLE =>
                    slave_o.dat(31) <= r_cyc;
                    slave_o.dat(30 downto 1) <= (others => '0');
                    slave_o.dat(0) <= r_cyc_tout_dis;
                    
                when c_CFG_REG_STATUS => 
                    slave_o.dat(31 downto 5)    <= (others => '0');
                    slave_o.dat( 4)             <= not pci_tx_fifo_empty;
                    slave_o.dat( 3 downto 1)    <= (others => '0');
                    slave_o.dat( 0)             <= pci_rx_fifo_full;

                when c_EB_CYCLE_MAX_TOUT =>
                    slave_o.dat <= std_logic_vector(r_eb_cyc_max_tout);

                when c_TX_DATA_COUNT => 
                    slave_o.dat(31 downto c_FIFO_COUNT_WIDHT)   <= (others => '0');
                    slave_o.dat(c_FIFO_COUNT_WIDHT-1 downto 0)  <= tx_fifo_rd_count;
                    
                when c_CFG_REG_SCRATCH =>
                    slave_o.dat <= r_scratch;
                    
                when others => 
                    slave_o.dat <= x"DEADC0DE";
            end case;
            
            -- writes
            if slave_i.stb = '1' and slave_i.we  = '1' then
                -- configuration registers
                case slave_i.adr(4 downto 2) is
                    when c_CFG_REG_CYCLE =>
                        if slave_i.dat(1) = '1' then
                            r_cyc_tout_dis <= slave_i.dat(0);
                        end if;
                        
                    when c_EB_CYCLE_MAX_TOUT =>
                        r_eb_cyc_max_tout <= unsigned(slave_i.dat);

                    when c_CFG_REG_SCRATCH => 
                        r_scratch <= slave_i.dat;
                    when others => null;
                    
                end case;
            end if;
        else
            -- TX FIFO READ
            slave_o.ack <= r_ack_dly;
            slave_o.dat <= pci_tx_fifo_dato;
        end if;
        
        -- delays
        r_ack_dly <= slave_i.stb;
        r_cyc_dly <= r_cyc;
        
    end if;
  end process;
  
  
  -- eb cycle timeout
  eb_cyc_tout <= '0' when (r_cyc_tout_counter < r_eb_cyc_max_tout)  else '1';
  
  p_cyc_timeout: process(clk_wb_i)
  begin
    if rising_edge(clk_wb_i) then
      if rstn_wb_i = '0' then
        r_cyc <= '0';
        r_cyc_tout_counter <= (others => '0');

      else

        -- host controls eb cycle
        if (slave_i.adr(5) = '0' and slave_i.adr(4 downto 2) = c_CFG_REG_CYCLE and 
            slave_i.stb = '1' and slave_i.we  = '1' and 
            slave_i.sel(3) = '1' and slave_i.dat(30) = '1') 
        then
          r_cyc <= slave_i.dat(31);
        -- ohterwise if cycle timeout occured then drop cycle  
        elsif eb_cyc_tout = '1' then 
          r_cyc <= '0';
        else -- hold
          r_cyc <= r_cyc;
        end if;
        
        -- cycle time out counter
        -- eb cycle not active or we have some activity from host to eb slave  or timeout disabled
        if (r_cyc = '0' or slave_i.stb = '1' or eb_cyc_tout = '1' or r_cyc_tout_dis = '1') then
          r_cyc_tout_counter <= (others => '0');
        
        -- count up when eb cycle opened
        else
          r_cyc_tout_counter <= r_cyc_tout_counter + 1;
        end if;
        
      end if;
    end if;
  end process;

  -- pass out for use with issp
  phy_cyc_o <= r_cyc;
  
  
  -- ===================================================================
  
  pci_tx_fifo_re   <= '1' when (slave_i.adr(5 downto 2) = c_TX_FIFO_DATA and
                                slave_i.we  = '0' and 
                                slave_i.stb = '1'
                                )
                       else '0';
  
  pci_tx_fifo_we <= '1' when (eb_tx_fifo_master_o.we  = '1' and 
                              eb_tx_fifo_master_o.stb = '1'
                             )
                    else '0';
  
  pci_tx_fifo_dati <= eb_tx_fifo_master_o.dat;


  -- signals from TX fifo back to eb_slave TX
  p_tx_ack: process(clk_xwb_i)
  begin
    if rising_edge(clk_xwb_i) then
        -- ack one clock after stb
        eb_tx_fifo_master_i.ack <= eb_tx_fifo_master_o.stb;
    end if;
  end process;
  
  eb_tx_fifo_master_i.err   <= '0';
  eb_tx_fifo_master_i.rty   <= '0';
  eb_tx_fifo_master_i.stall <= pci_tx_fifo_full;
  eb_tx_fifo_master_i.int   <= '0';
  eb_tx_fifo_master_i.dat   <= (others => '0');
    

-- just in case reset tx fifo on new EB packet
tx_fifo_rst <= '0' when (rstn_wb_i = '0' or (r_cyc_dly = '0' and r_cyc = '1')) else '1';


pci_tx_fifo: entity work.generic_async_fifo
  generic map(
    g_data_width => 32, -- natural;
    g_size       => g_fifo_size, -- natural;
    g_show_ahead => true, -- boolean := false;

    -- Read-side flag selection
    g_with_rd_empty        => true , -- boolean := true;   -- with empty flag
    g_with_rd_full         => false, -- boolean := false;  -- with full flag
    g_with_rd_almost_empty => false, -- boolean := false;
    g_with_rd_almost_full  => false, -- boolean := false;
    g_with_rd_count        => true, -- boolean := false;  -- with words counter

    g_with_wr_empty        => false, -- boolean := false;
    g_with_wr_full         => true , -- boolean := true;
    g_with_wr_almost_empty => false, -- boolean := false;
    g_with_wr_almost_full  => false, -- boolean := false;
    g_with_wr_count        => false, -- boolean := false;

    g_almost_empty_threshold => 16, -- integer;  -- threshold for almost empty flag
    g_almost_full_threshold  => 16  -- integer   -- threshold for almost full flag
    )
  port map (
    rst_n_i => tx_fifo_rst, -- in std_logic := '1';


    -- write port - eb slave source
    clk_wr_i => clk_xwb_i, -- in std_logic;
    d_i      => pci_tx_fifo_dati, -- in std_logic_vector(g_data_width-1 downto 0);
    we_i     => pci_tx_fifo_we, -- in std_logic;

--    wr_empty_o        => , --  out std_logic;
    wr_full_o         => pci_tx_fifo_full, --  out std_logic;
--    wr_almost_empty_o => , --  out std_logic;
--    wr_almost_full_o  => , --  out std_logic;
--    wr_count_o        => , --  out std_logic_vector(f_log2_size(g_size)-1 downto 0);

    -- read port - pci wb side
    clk_rd_i => clk_wb_i, -- in  std_logic;
    q_o      => pci_tx_fifo_dato, -- out std_logic_vector(g_data_width-1 downto 0);
    rd_i     => pci_tx_fifo_re, -- in  std_logic;

    rd_empty_o        => pci_tx_fifo_empty, -- out std_logic;
--    rd_full_o         => , -- out std_logic;
--    rd_almost_empty_o => , -- out std_logic;
--    rd_almost_full_o  => , -- out std_logic;
      rd_count_o        => tx_fifo_rd_count  -- out std_logic_vector(f_log2_size(g_size)-1 downto 0)
    );

  -- ===================================================================

  -- RX FIFO write
  pci_rx_fifo_we   <= '1' when (slave_i.adr(5 downto 2) = c_RX_FIFO_DATA and
                                slave_i.we  = '1' and 
                                slave_i.stb = '1'
                                )
                       else '0';
  pci_rx_fifo_dati <= slave_i.dat;

pci_rx_fifo: entity work.generic_async_fifo
  generic map(
    g_data_width => 32, -- natural;
    g_size       => g_fifo_size, -- natural;
    g_show_ahead => false, -- boolean := false;

    -- Read-side flag selection
    g_with_rd_empty        => true , -- boolean := true;   -- with empty flag
    g_with_rd_full         => false, -- boolean := false;  -- with full flag
    g_with_rd_almost_empty => false, -- boolean := false;
    g_with_rd_almost_full  => false, -- boolean := false;
    g_with_rd_count        => false, -- boolean := false;  -- with words counter

    g_with_wr_empty        => false, -- boolean := false;
    g_with_wr_full         => true , -- boolean := true;
    g_with_wr_almost_empty => false, -- boolean := false;
    g_with_wr_almost_full  => false, -- boolean := false;
    g_with_wr_count        => false, -- boolean := false;

    g_almost_empty_threshold => 16, -- integer;  -- threshold for almost empty flag
    g_almost_full_threshold  => 16  -- integer   -- threshold for almost full flag
    )
  port map (
    rst_n_i => rstn_wb_i, -- in std_logic := '1';

    -- write port - eb slave source
    clk_wr_i => clk_wb_i, -- in std_logic;
    d_i      => pci_rx_fifo_dati, -- in std_logic_vector(g_data_width-1 downto 0);
    we_i     => pci_rx_fifo_we, -- in std_logic;

--    wr_empty_o        => , --  out std_logic;
    wr_full_o         => pci_rx_fifo_full, --  out std_logic;
--    wr_almost_empty_o => , --  out std_logic;
--    wr_almost_full_o  => , --  out std_logic;
--    wr_count_o        => , --  out std_logic_vector(f_log2_size(g_size)-1 downto 0);

    -- read port - pci wb side
    clk_rd_i => clk_xwb_i, -- in  std_logic;
    q_o      => pci_rx_fifo_dato, -- out std_logic_vector(g_data_width-1 downto 0);
    rd_i     => pci_rx_fifo_re, -- in  std_logic;

    rd_empty_o        => pci_rx_fifo_empty -- out std_logic;
--    rd_full_o         => , -- out std_logic;
--    rd_almost_empty_o => , -- out std_logic;
--    rd_almost_full_o  => , -- out std_logic;
--    rd_count_o        =>   -- out std_logic_vector(f_log2_size(g_size)-1 downto 0)
    );    
    
    
    
  -- read what was written by PC to FIFO 
  -- only when there is something to read and slave is ready to accept it
  pci_rx_fifo_re <= '1' when  (eb_rx_stream_master_i.stall = '0' and 
                               pci_rx_fifo_empty           = '0'
                              )
                   else '0';                              

  
  -- push data from FIFO to slave only when there is something to read and slave is ready to accept it
  --eb_rx_stream_master_o.stb <= not pci_rx_fifo_empty and not eb_rx_stream_master_i.stall;
  p_rx_sm_stb_cyc: process(clk_xwb_i)
  begin
    if rising_edge(clk_xwb_i) then
        -- push data from FIFO to slave only when new data was read
        eb_rx_stream_master_o.stb <= pci_rx_fifo_re;
        -- sync to xwb clock domain
        eb_rx_stream_master_cyc_sync <= r_cyc & eb_rx_stream_master_cyc_sync(eb_rx_stream_master_cyc_sync'left downto 1);
    end if;
  end process;

  -- comes from 
  eb_rx_stream_master_o.cyc <= eb_rx_stream_master_cyc_sync(0);
  
  -- dont care
  eb_rx_stream_master_o.adr <= (others => '0');
  eb_rx_stream_master_o.sel <= x"F";
  eb_rx_stream_master_o.we  <= '1';
  eb_rx_stream_master_o.dat <= pci_rx_fifo_dato;

  EB : eb_slave_top
    generic map(
      g_sdb_address    => g_sdb_address(31 downto 0),
      g_timeout_cycles => g_timeout_cycles)
    port map(
      clk_i        => clk_xwb_i ,
      nRst_i       => rstn_xwb_i,

      EB_RX_o      => eb_rx_stream_master_i,
      EB_RX_i      => eb_rx_stream_master_o,

      EB_TX_i      => eb_tx_fifo_master_i,
      EB_TX_o      => eb_tx_fifo_master_o,

      skip_stb_o   => open,
      skip_stall_i => '0',

      WB_master_i  => eb_slave_int_master_i,
      WB_master_o  => eb_slave_int_master_o,

      WB_config_i  => cc_dummy_slave_in, --cfg_slave_i,
      WB_config_o  => open, --cfg_slave_o,

      my_mac_o     => open,
      my_ip_o      => open,
      my_port_o    => open
    );  
    
    eb_slave_int_master_i   <= master_i;
    master_o                <= eb_slave_int_master_o;
  
  
end rtl;
