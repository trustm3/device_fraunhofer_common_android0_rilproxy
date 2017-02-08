--
-- This file is part of trust|me
-- Copyright(c) 2013 - 2017 Fraunhofer AISEC
-- Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V.
--
-- This program is free software; you can redistribute it and/or modify it
-- under the terms and conditions of the GNU General Public License,
-- version 2 (GPL 2), as published by the Free Software Foundation.
--
-- This program is distributed in the hope it will be useful, but WITHOUT
-- ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
-- FITNESS FOR A PARTICULAR PURPOSE. See the GPL 2 license for more details.
--
-- You should have received a copy of the GNU General Public License along with
-- this program; if not, see <http://www.gnu.org/licenses/>
--
-- The full GNU General Public License is included in this distribution in
-- the file called "COPYING".
--
-- Contact Information:
-- Fraunhofer AISEC <trustme@aisec.fraunhofer.de>
--

require "rilcommands"


dc_udp_dport = Field.new("udp.dstport")
dc_udp_sport = Field.new("udp.srcport")
 

p_ril = Proto ("ril","RIL PARCEL")

--p_ril.prefs["udp_port_start"]	= Pref.string("UDP port range start", "12345", "First UDP port to decode as this protocol")
--p_ril.prefs["udp_port_end"]	= Pref.string("UDP port range end", "12348", "Last UDP port to decode as this protocol")

local f = p_ril.fields

f.parcel_size 	= ProtoField.uint32("ril.psize", "Parcelsize")
f.event_id	= ProtoField.uint32("ril.eventid", "Event ID")
f.packet_serial	= ProtoField.uint32("ril.packetserial", "Serial")
f.parcel_data   = ProtoField.bytes("ril.data", "Data")


p_ril.prefs["udp_port"] = Pref.uint("UDP Port", 12345, "UDP Port for RILPROXY")

--ril dissector function
function p_ril.dissector (buffer, pinfo, tree)
	local subtree = tree:add (p_ril,buffer())
	local offset = 0
	local parcel_size = buffer (offset,4)
	local event_id    = buffer ( 4,4)
	local packet_serial = buffer (8,4)
	
	local udpsport = dc_udp_sport()
	local udpdport = dc_udp_dport()

	pinfo.cols.protocol = p_ril.name
	pinfo.cols.info = p_ril.name

	subtree:add (f.parcel_size,parcel_size)
	subtree:append_text(",ParcelSize: " .. parcel_size:uint())
	--pinfo.cols.info:append(",ParcelSize: " .. parcel_size:uint())

	if (event_id:le_uint() == 0) then
		subtree:add_le (f.event_id,event_id )
		subtree:append_text(", Reply for " )
		pinfo.cols.info:append(", Reply for " )
		subtree:add_le (f.packet_serial,packet_serial)
		subtree:add (f.parcel_data,buffer(12))
		subtree:append_text(", Serial: " .. packet_serial:le_uint())
		pinfo.cols.info:append(", Serial: " .. packet_serial:le_uint())

	elseif (tostring(udpdport) == "12346") then
			subtree:add_le (f.event_id,packet_serial)
			subtree:add_le (f.packet_serial,event_id)
			subtree:add (f.parcel_data,buffer(12))

			subtree:append_text(", UNSOL Event ID: " .. packet_serial:le_uint())
			subtree:append_text(", " .. rilunsolmessages[packet_serial:le_uint()][1])

			pinfo.cols.info:append(" , UNSOL Event ID: " .. packet_serial:le_uint())
			pinfo.cols.info:append(" , " .. rilunsolmessages[packet_serial:le_uint()][1])
		
		else

	 		--if (tostring(udpsport) == "12347") then
				subtree:add_le (f.event_id,event_id )
				subtree:append_text(", Event ID: " .. event_id:le_uint())
				subtree:append_text("," .. rilsolmessages[event_id:le_uint()][1])
			
				pinfo.cols.info:append("," .. rilsolmessages[event_id:le_uint()][1])
				
				subtree:add_le (f.packet_serial,packet_serial)
				
				subtree:add (f.parcel_data,buffer(12))
				subtree:append_text(", Serial: " .. packet_serial:le_uint())
				
				pinfo.cols.info:append(", Serial: " .. packet_serial:le_uint())
	end

	if parcel_size:uint() ~= (buffer(12):len() +8) then
		pinfo.cols.info:append(" malformed / fragmented Buffer, Size:".. (buffer(12):len()+8))
	end	
end

-- Initialization routine
function p_ril.init()
	local udptab = DissectorTable.get("udp.port")

	udptab:add(12345, p_ril)
	udptab:add(12346, p_ril)
	udptab:add(12347, p_ril)
	udptab:add(12348, p_ril)
end

