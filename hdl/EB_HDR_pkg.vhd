-------------------------------------------------------------------------------
-- White Rabbit Switch / GSI BEL
-------------------------------------------------------------------------------
--
-- unit name: IPV4/UDP/Etherbone Header Package
--
-- author: Mathias Kreider, m.kreider@gsi.de
--
-- date: $Date:: $:
--
-- version: $Rev:: $:
--
-- description: <file content, behaviour, purpose, special usage notes...>
-- <further description>
--
-- dependencies: <entity name>, ...
--
-- references: <reference one>
-- <reference two> ...
--
-- modified by: $Author:: $:
--
-------------------------------------------------------------------------------
-- last changes: <date> <initials> <log>
-- <extended description>
-------------------------------------------------------------------------------
-- TODO: <next thing to do>
-- <another thing to do>
--
-- This code is subject to GPL
-------------------------------------------------------------------------------

---! Standard library
library IEEE;
--! Standard packages    
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

--! Additional library
library work;
--! Additional packages    
--use work.XXX.all;


package EB_HDR_PKG is

constant c_local_ip               : std_logic_vector(31 downto 0) := x"C0A80164"; -- fixed address for now. 192.168.1.100 

--define ETH frame hdr
type ETH_HDR is record

   -- RX only, use constant fields for TX --------
   PRE_SFD    : std_logic_vector((8*8)-1 downto 0);   
   DST         : std_logic_vector((6*8)-1 downto 0); 
   SRC         : std_logic_vector((6*8)-1 downto 0);
   TPID           : std_logic_vector((2*8)-1 downto 0);
   TCI           : std_logic_vector((2*8)-1 downto 0);   
   
end record;

--define IPV4 header
type IPV4_HDR is record

   -- RX only, use constant fields for TX --------
   VER       : std_logic_vector(3 downto 0);   
   IHL     : std_logic_vector(3 downto 0); 
   TOS     : std_logic_vector(7 downto 0); 
   TOL       : std_logic_vector(15 downto 0);        
   ID      : std_logic_vector(15 downto 0);
   FLG     : std_logic_vector(2 downto 0);
   FRO     : std_logic_vector(12 downto 0);
   TTL     : std_logic_vector(7 downto 0);
   PRO     : std_logic_vector(7 downto 0);
   SUM     : std_logic_vector(15 downto 0);
   SRC       : std_logic_vector(31 downto 0);
   DST       : std_logic_vector(31 downto 0);
 --- options (optional) here
end record;

--define UDP header
type UDP_HDR is record
   SRC_PORT,   
   DST_PORT,   
   MLEN,         
   SUM        : std_logic_vector(15 downto 0);
end record;

--define Etherbone header
type EB_HDR is record
    EB_MAGIC    : std_logic_vector(15 downto 0);
    
    VER             : std_logic_vector(3 downto 0);
    RESERVED1    : std_logic_vector(3 downto 0);
    ADD_SIZE    : std_logic_vector(3 downto 0);
    PORT_SIZE   : std_logic_vector(3 downto 0);
    
    ADD_STATUS    : std_logic_vector(31 downto 0);
    
    RESERVED2    : std_logic_vector(2 downto 0);
    RD_FIFO        : std_logic;
    RD_CNT        : std_logic_vector(11 downto 0);
    
    RESERVED3    : std_logic_vector(2 downto 0);    
    WR_FIFO        : std_logic;
    WR_CNT        : std_logic_vector(11 downto 0);
end record;

--conversion function prototyes    
function TO_STD_LOGIC_VECTOR(X : ETH_HDR)
return std_logic_vector;

function TO_ETH_HDR(X : std_logic_vector)
return ETH_HDR;

function INIT_ETH_HDR(SRC_MAC : std_logic_vector)
return ETH_HDR;

function TO_STD_LOGIC_VECTOR(X : IPV4_HDR)
return std_logic_vector;

function TO_IPV4_HDR(X : std_logic_vector)
return IPV4_HDR;

function INIT_IPV4_HDR(SRC_IP : std_logic_vector)
return IPV4_HDR;

function TO_STD_LOGIC_VECTOR(X : UDP_HDR)
return std_logic_vector;

function TO_UDP_HDR(X : std_logic_vector)
return UDP_HDR;

function INIT_UDP_HDR
return UDP_HDR;

function TO_STD_LOGIC_VECTOR(X : EB_HDR)
return std_logic_vector;

function TO_EB_HDR(X : std_logic_vector)
return EB_HDR;

function INIT_EB_HDR
return EB_HDR;

end EB_HDR_PKG;

package body EB_HDR_PKG is

--conversion functions

-- output to std_logic_vector
function TO_STD_LOGIC_VECTOR(X : ETH_HDR)
return std_logic_vector is
    variable tmp : std_logic_vector(191 downto 0) := (others => '0');
    begin
  tmp := X.PRE_SFD & X.DST & X.SRC & X.TPID & X.TCI; 
  return tmp;
end function TO_STD_LOGIC_VECTOR;


function TO_STD_LOGIC_VECTOR(X : IPV4_HDR)
return std_logic_vector is
    variable tmp : std_logic_vector(159 downto 0) := (others => '0');
    begin
  tmp := X.VER & X.IHL & X.TOS & X.TOL & X.ID & X.FLG & X.FRO & X.TTL & X.PRO & X.SUM & X.SRC & X.DST ; 
  return tmp;
end function TO_STD_LOGIC_VECTOR;

function TO_STD_LOGIC_VECTOR(X : UDP_HDR)
return std_logic_vector is
    variable tmp : std_logic_vector(63 downto 0) := (others => '0');
    begin
        tmp :=  X.SRC_PORT & X.DST_PORT & X.MLEN & X.SUM;
    return tmp;
end function TO_STD_LOGIC_VECTOR;

function TO_STD_LOGIC_VECTOR(X : EB_HDR)
return std_logic_vector is
    variable tmp : std_logic_vector(95 downto 0) := (others => '0');
    begin
              tmp :=  X.EB_MAGIC & X.VER & X.RESERVED1 & X.ADD_SIZE & X.PORT_SIZE & X.ADD_STATUS & X.RESERVED2 
                & X.RD_FIFO  & X.RD_CNT  & X.RESERVED3  & X.WR_FIFO & X.WR_CNT;

    return tmp;
end function TO_STD_LOGIC_VECTOR;

-- Give back initialised IPV4 Header. Needs local IP, add valid checksum
function INIT_ETH_HDR(SRC_MAC : std_logic_vector)
return ETH_HDR is
variable tmp : ETH_HDR;    
    begin
        tmp.PRE_SFD  := x"55555555555555D5"; -- 4
        tmp.DST      := (others => '0');     -- 4
        tmp.SRC      := SRC_MAC;    -- 8
        tmp.TPID     := x"8100"; --type ID
        tmp.TCI      := x"E000"; --priority 7
    return tmp;
end function INIT_ETH_HDR;




function INIT_IPV4_HDR(SRC_IP : std_logic_vector) --loads constants into a given IPV4_HDR record
return IPV4_HDR is
variable tmp : IPV4_HDR;


    begin
        tmp.VER :=     x"4"; -- 4
        tmp.IHL :=     x"5"; -- 4
        tmp.TOS :=  x"00";    -- 8
        
        tmp.TOL := (others => '0');
                     
        tmp.ID  :=     (others => '0');--16
        
        tmp.FLG :=     "010";--
        tmp.FRO :=     (others => '0');-- 0
        
        tmp.TTL :=     x"40";    -- 64 Hops
        tmp.PRO :=     x"11";    -- UDP
        
        tmp.SUM := (others => '0');        --16
        
        tmp.SRC := SRC_IP;        --32 -- SRC is already known
        tmp.DST := (others => '0');        --32
        
    return tmp;
end function INIT_IPV4_HDR;


-- cast to records

function TO_ETH_HDR(X : std_logic_vector)
return ETH_HDR is
    variable tmp : ETH_HDR;
    begin
		tmp.PRE_SFD := X(191 downto 128);
		tmp.DST 	:= X(127 downto 80);
		tmp.SRC 	:= X(79 downto 32);
		tmp.TPID 	:= X(31 downto 16);
		tmp.TCI 	:= X(15 downto 0);
	return tmp;
end function TO_ETH_HDR;




function TO_IPV4_HDR(X : std_logic_vector)
return IPV4_HDR is
    variable tmp : IPV4_HDR;
    begin
        tmp.VER := X(159 downto 156);
		tmp.IHL := X(155 downto 152);
		tmp.TOS := X(151 downto 144);
		tmp.TOL := X(143 downto 128);
		tmp.ID 	:= X(127 downto 112);
		tmp.FLG := X(111 downto 99);
		tmp.FRO := X(98 downto 96);
		tmp.TTL := X(95 downto 88);
		tmp.PRO := X(87 downto 80);
		tmp.SUM := X(79 downto 64);
		tmp.SRC := X(63 downto 32);
		tmp.DST := X(31 downto 0);

  return tmp;
end function TO_IPV4_HDR;

function TO_UDP_HDR(X : std_logic_vector)
return UDP_HDR is
    variable tmp : UDP_HDR;
    begin
        tmp.SRC_PORT 	:= X(63 downto 48);
		tmp.DST_PORT 	:= X(47 downto 32);
		tmp.MLEN 		:= X(31 downto 16);
		tmp.SUM 		:= X(15 downto 0);
	return tmp;
end function TO_UDP_HDR;

-- Give back UDP Header initialised to zero. Add ports, length and valid checksum 

function INIT_UDP_HDR
return UDP_HDR is
    variable tmp : UDP_HDR;
    begin
        tmp.SRC_PORT    := (others => 'Z'); --16
        tmp.DST_PORT    := (others => '1'); --16
        tmp.MLEN        := (others => '0'); --16
        tmp.SUM         := (others => '0'); --16
    return tmp;
end function INIT_UDP_HDR;


function TO_EB_HDR(X : std_logic_vector)
return EB_HDR is
    variable tmp : EB_HDR;
    begin
		tmp.EB_MAGIC 	:= X(95 downto 80);
		tmp.VER 		:= X(79 downto 76);
		tmp.RESERVED1 	:= X(75 downto 72);
		tmp.ADD_SIZE 	:= X(71 downto 68);
		tmp.PORT_SIZE 	:= X(67 downto 64);
		tmp.ADD_STATUS 	:= X(63 downto 32);
		tmp.RESERVED2 	:= X(31 downto 29);
		tmp.RD_FIFO 	:= X(28);
		tmp.RD_CNT 		:= X(27 downto 16);
		tmp.RESERVED3 	:= X(15 downto 13);
		tmp.WR_FIFO 	:= X(12);
		tmp.WR_CNT 		:= X(11 downto 0);

    return tmp;
end function TO_EB_HDR;

-- Give back Etherbone Header initialised to zero. Add Etherbone data
function INIT_EB_HDR
return EB_HDR is
    variable tmp : EB_HDR;
    begin
        tmp.EB_MAGIC    := (others => 'Z'); --16
        
        tmp.VER         := (others => '1');    --  4
        tmp.RESERVED1   := (others => '0');-- reserved 3bit                
        tmp.ADD_SIZE    := (others => '0'); --  4
        tmp.PORT_SIZE   := (others => '0'); --  4
        tmp.ADD_STATUS  := (others => '0'); -- 32
        
        tmp.RESERVED2   := (others => '0');-- reserved 3bit                    
        tmp.RD_FIFO     := '0';            -- 1
        tmp.RD_CNT      := (others => '0');    --12
        
        tmp.RESERVED3   := (others => '0');-- reserved 3bit                   --  3        
        tmp.WR_FIFO     := '0';           --  1    
        tmp.WR_CNT      := (others => '0'); -- 12
    return tmp;
end function INIT_EB_HDR;



----------------------------------------------------------------------------------

end package body;



