# Callback functions
Agent/DTNAgent instproc indData {sid did rid cos options lifespan adu} {
    $self instvar node_
    $self instvar $adu
    puts "node [$node_ id] ($did) received bundle from \
              $sid : '$adu'"    
}

Agent/DTNAgent instproc indSendError {sid did rid cos options lifespan adu} {
    $self instvar node_
    puts "node [$node_ id] ($sid) got send error on bundle to \
              $did : '$adu'"
}

Agent/DTNAgent instproc indSendToken {binding sendtoken} {
    $self instvar node_
#    puts "node [$node_ id] Send binding $binding bound to token $sendtoken"
    if { [string equal $binding "deleteme" ] } {
        $self cancel $sendtoken
    }
}

Agent/DTNAgent instproc indRegToken {binding regtoken} {
    $self instvar node_
    puts "node [$node_ id] Reg binding $binding bound to token $regtoken"
}

# ======================================================================
# Define options
# ======================================================================
set val(chan)           Channel/WirelessChannel    ;# channel type
set val(prop)           Propagation/FreeSpace  			;# radio-propagation model TwoRayGround
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue		   ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                    ;# max packet in ifq
set val(nn)             [lindex $argv 31]          ;# number of mobilenode_
set val(rp)             DumbAgent                  ;# routing protocol
# ======================================================================
# Main Program
# ======================================================================



#Show Entries
puts "List of user entries"
puts "------------------------------------------------"
for {set i 0} {$i <  $argc } {incr i} {
puts "[lindex $argv $i] = [lindex $argv [expr $i + 1]]"
set i [expr $i + 1]
}
puts "------------------------------------------------"

#gets stdin


#
# Initialize Global Variables
#
#foreach cl [PacketHeader info subclass] {
#        puts $cl
#       }
#remove-all-packet-headers
remove-packet-header PBC LRWPAN XCP PGM PGM_SPM PGM_NAK NV

remove-packet-header LDP MPLS rtProtoLS Ping TFRC TFRC_ACK Diffusion RAP AOMDV
remove-packet-header AODV SR TORA IMEP MIP IPinIP Encap
remove-packet-header  HttpInval SRMEXT SRM aSRM mcastCtrl CtrMcast rtProtoDV GAF Snoop SCTP
remove-packet-header TCPA TCP IVS RTP
remove-packet-header Message Resv QS UMP
remove-packet-header Src_rt Flags





Mac/802_11 set basicRate_ 1e6
Mac/802_11 set dataRate_  110e6
Mac/802_11 set bandwidth_ 110e6
#Phy/WirelessPhy set bandwidth_ 11e6

Antenna/OmniAntenna set X_ 0
Antenna/OmniAntenna set Y_ 0
Antenna/OmniAntenna set Z_ 1.5
Antenna/OmniAntenna set Gt_ 1.0  ;#be aware the Gt_ and Gr_ are setted as the
Antenna/OmniAntenna set Gr_ 1.0  ;#fixed value 1.0
Phy/WirelessPhy set freq_ 2.4e9
Phy/WirelessPhy set L_ 1.0
Phy/WirelessPhy set Pt_ 0.3196e-5  ;#power needed to have 100m of distance
#Phy/WirelessPhy set Pt_ 0.09588e-5	;#power needed to have 30m of distance
Phy/WirelessPhy set RXThresh_ 3.16e-14
Phy/WirelessPhy set CSThresh_ 3.16e-14   ;#Sensivity=-105 dbm
Phy/WirelessPhy set CPThresh_ 10


set ns_	[new Simulator]
$ns_ use-scheduler Heap

#$ns_ use-newtrace
set tracefd     [open dtn.tr w]
$ns_ trace-all $tracefd




# Initialize generated packets sizes 
for {set i 0} {$i <=  $val(nn) } {incr i} {
set generated_packets_tab($i) 0
}

set avg_data_size [open "avg_data_size.dat" w]
set avg_epidemic_data_size [open "avg_epidemic_data_size.dat" w]
set avg_stat_data_size [open "avg_stat_data_size.dat" w]
set estimated_nn [open "estimated_nn.dat" w]


set fg [open "fg.dat" w]
set td [open "td.dat" w]
set total_d [open "total_d.dat" w]
set total_del [open "total_del.dat" w]
set avg_ld [open "avg_ld.dat" w]
set dest_id [open "dest_id.dat" w]
set avg_dl [open "avg_delay.dat" w]
set track_f [open "track_f.dat" w]



# set up topography object
set topo       [new Topography]
$topo load_flatgrid [lindex $argv 33] [lindex $argv 35]

#
# Create God
#
set god_ [create-god [expr $val(nn) + 1]]
puts "loading the scene file !"

#
#  Create the specified number of mobilenode_ [$val(nn)] and "attach" them
#  to the channel. 
#  Here two node_ are created : node(0) and node(1)

# configure node

        $ns_ node-config -adhocRouting $val(rp) \
			 -llType $val(ll) \
			 -macType $val(mac) \
			 -ifqType $val(ifq) \
			 -ifqLen $val(ifqlen) \
			 -antType $val(ant) \
			 -propType $val(prop) \
			 -phyType $val(netif) \
			 -channelType $val(chan) \
			 -topoInstance $topo \
			 -agentTrace OFF \
			 -routerTrace OFF \
			 -macTrace OFF \
			 -movementTrace OFF

for {set i 0} {$i <= $val(nn) } {incr i} { set node_($i) [$ns_ node] 
					   $node_($i) random-motion 0
}

source ./scene_ns

# Create and attach agents.

#Defining  CBR Applications
set lf [lindex $argv 5]
# Don't forget the bid expiration time, it is lf * 100
set cbr_i [lindex $argv 3]
set pk_size [lindex $argv 27]
set sim_dur [lindex $argv 7]
set max_stat_messages [lindex $argv 39]
set time_to_wait_for_new_stat [lindex $argv 41]
set start_uid 	[lindex $argv 37] 				  
set end_uid [lindex $argv 43] 						
						
	
for { set i 0} {$i <= $val(nn)} {incr i} {set dtn($i) [new Agent/DTNAgent]}
for { set i 0} {$i <= $val(nn)} {incr i} { $ns_ attach-agent $node_($i) $dtn($i)}
for { set i 0} {$i <= $val(nn)} {incr i} { $dtn($i) region "REGION1"}
for { set i 0} {$i <= $val(nn)} {incr i} {
						# Dtn node id
						$dtn($i) rule $i
						
						# Max messages in the local buffer
						$dtn($i) config max_bundles_in_local_buffer [lindex $argv 21]
						# Maximum number of neigbors
						$dtn($i) config max_neighbors 1000.0
						# Maximum number of hops
						$dtn($i) config max_hop_count 0.0
						# Max Ids Count
						$dtn($i) config max_ids_count 9999.0	
						# Max UID length
						$dtn($i) config max_uid_length 100.0

						# Hello Interval
						$dtn($i) config hello_interval 30.0
						# Delivered bundles cleaning interval
						$dtn($i) config delivered_bundles_cleaning_interval 6000.0
						# Block resend interva;
						$dtn($i) config block_resend_interval 3.0
						# maximum neighbor abcense time, for neighbors check interval look at bmflags.h
						$dtn($i) config neighbors_check_interval 31.0 

						# The current Routing protocol
						$dtn($i) config activate_routing 1
						# Max Sprying time
						$dtn($i) config max_sprying_time 50.0

						# Drop Policy

						# 1: Drop Tail
						# 2: Drop front
						# 3: Drop Youngest
						# 4: Drop Oldest
						# 5: Drop and Maximize Total Delivery Rate Reference
						# 6: Drop and Minimize Total Delay Reference
						# 7: Drop and Maximize Total Delivery Rate Approximation
						# 8: Drop and Minimize Total Delay Approximation 

						$dtn($i) drop_policie [lindex $argv 15]
						$dtn($i) drop_oldest_bin [lindex $argv 45]	 
						# Scheduling policy to use
						# 1: arrange_bundles_to_send_according_to_reference_delivery_metric
						# 2: arrange_bundles_to_send_according_to_reference_delay_metric
						# 3: arrange_bundles_to_send_according_to_history_based_delivery_metric
						# 4: arrange_bundles_to_send_according_to_history_based_delay_metric
						# 5: arrange_bundles_to_send_according_to_flooding_based_delivery_metric
						# 6: rrange_bundles_to_send_according_to_flooding_based_delay_metric
					 	$dtn($i) config scheduling_tye [lindex $argv 17]
						# Possibility to setup an infinit ttl
						$dtn($i) config infinite_ttl [lindex $argv 13]

						# Activate or not the ack mechanism
						$dtn($i) config activate_ack_mechanism 0
						# How many times we can resend a block
						$dtn($i) max_block_retransmission_number 0
						# activate or not the anti packets mechanism
						$dtn($i) config anti_packet_mechanism 0 

						# Either we maintain statistics or not
						$dtn($i) config maintain_stat [lindex $argv 11]
						# Either we use the reference parameters or not
						$dtn($i) config reference [lindex $argv 29]
						# The Axe subdivition to use
						$dtn($i) config axe_subdivision [lindex $argv 23]
						# The stat Axe length
						$dtn($i) config axe_length [lindex $argv 25]

						#Sprying policy
						# 0: Pushing Strategy
						# 1: Asking stat based on a set of id(s)
						# 2: Asking stat based on a set of id+version(s)
						$dtn($i) stat_sprying_policy [lindex $argv 19]
						
						# Max Number of stat Messages
						$dtn($i) config max_number_of_stat_messages $max_stat_messages
						$dtn($i) config time_to_wait_until_using_new_stat $time_to_wait_for_new_stat
						$dtn($i) config start_from_uid $start_uid 
						$dtn($i) config stop_at_uid $end_uid 
				  }

#Defining  CBR Applications
# ../setdest/setdest -v 2 -n 100 -m 5 -M 7 -t 14400 -p 10 -x 5100 -y 5100 >scene_ns

set lf [lindex $argv 5]
# Don't forget the bid expiration time, it is lf * 100
set cbr_i [lindex $argv 3]
set pk_size [lindex $argv 27]
set sim_dur [lindex $argv 7]


#argv 1 number of sources 0
#argv 3 cbr interval 1
#argv 5 ttl 2
#argv 7 simulation duration 3
#argv 9 max generated packets / source 4
#argv 11 maintain stat  5
#argv 13 infinite ttl (0/1) 6
#argv 15 drop policy number 7
#argv 17 scheduling policy number 8
#argv 19 stat sprying policy number 9
#argv 21 buffer size 10 
#argv 23 axe subdivision 11
#argv 25 axe length 12
#argv 27 packet size 13
#argv 29 reference policy (0 or 1) 14
#argv 31 number of nodes  15
#argv 33 grid size x   16
#argv 35 grid size y  17



#set number_of_sources [expr $val(nn) + 1]
set number_of_sources [lindex $argv 1]
set max_packets [lindex $argv 9]

set modulo [expr $val(nn) + 1]

for { set i 0} { $i <=  $val(nn)  } {incr i} { 
	set app($i) [new Application/Traffic/CBR]
	$app($i) set packetSize_ $pk_size
	$app($i) set interval_   $cbr_i
	$app($i) set random_     0
	$app($i) set maxpkts_    $max_packets
	set generated_packets_tab($i) $max_packets
	$app($i) attach-agent $dtn($i)
	$dtn($i) app src "REGION1,$node_($i):0"
	set r [expr rand()]
	set r2 [expr $r * 100]
	set r3 [expr round($r2)]
	set destination [expr $r3 % $modulo]
	if { $destination == $i } { 
		if { $destination > 0 } {
 			set destination [expr $destination -1]
		} else { set destination [expr $destination + 1] }
	}
	puts "Source: REGION1,$node_($i):0   destination: REGION1,$node_($destination):0"
	puts $dest_id "$destination"
	$dtn($destination) register DEFER bindM1 REGION1,$node_($destination):0
	$dtn($i) app dest "REGION1,$node_($destination):0"
	$dtn($i) app rpt_to "REGION1,$node_($i):0"
	$dtn($i) app cos "NORMAL"
	$dtn($i) app options "NONE"
	$dtn($i) app lifespan $lf
}


#########################################################
# PairWiseAvg Algorithm Setup
#########################################################
$dtn(20) config  current_number_of_nodes 1

proc log_number_of_nodes {} {

	global estimated_nn dtn ns_
	set enc [$dtn(20) set estimated_number_of_nodes]	
	set tawa [$ns_ now]
	puts $estimated_nn $enc
	$ns_ at [expr $tawa + 100] "log_number_of_nodes"
}

$ns_ at 0 "log_number_of_nodes"


#########################################################
# Scheduling the application to start generating packets
#########################################################

set last_time 0
for { set i 0} {$i < $number_of_sources } {incr i} { 
	global last_time
	set t [expr rand()]
	set t2 [expr $t * 10]
	set t3 [expr round($t2)]
	set time_to_start [expr $t3 % 100 + $last_time]
	set time_to_start [expr $time_to_start + 1]
	set last_time $time_to_start
	puts "Starting the application: $i at $time_to_start"
	$ns_ at  [expr $time_to_start]  "$app($i) start"
}


#########################################################
# Setup a random motion
#########################################################


proc set_random {} {
	global val node_
	for { set i 0} {$i <= $val(nn)} {incr i} { 
		$node_($i) random-motion 1
	}
}
proc start_move {} {
	global val node_
	for { set i 0} {$i <= $val(nn)} {incr i} { 
		$node_($i) start	
	}
}

#$ns_ at 0.0 "start_move"

#########################################################
# Calculating the average delivery rate
#########################################################

set fgg 0.0
set tdd 0.0

proc generated_packets {} {
	global  dtn fg val fgg generated_packets_tab number_of_sources
	set sum 0
	
	for { set i 0} {$i < $number_of_sources } {incr i} { 
	set sum [expr $sum + $generated_packets_tab($i)]
	}

	set fgg $sum
	puts $fg "$sum"
}

proc delivered_packets {} {
	global val dtn td tdd 
	set sum 0
	
	for { set i 0} {$i <= $val(nn) } {incr i} { 
	set vv [$dtn($i) set delivered_bundles]
	set sum [expr $sum + [$dtn($i) set delivered_bundles]]
	puts " Delivred Bundles $i $vv"
	}

	set tdd $sum
	puts $td "$sum"


}

proc DeliveryRate {} {
	global tdd fgg total_d number_of_sources start_uid stop_at_uid end_uid
	set tmp1 [expr $end_uid - $start_uid]
	set tmp2 [expr $tmp1 + 1]
	set tmp3 [expr $tmp2 * $number_of_sources]
	set avg 0
	if { $tmp3 > 0 } {
		set avg [expr $tdd.0 / $tmp3]
	}
	puts $total_d "$avg"
}

#########################################################
# Record deleted packets
#########################################################

proc record_total_deleted_packets {} {
	global val dtn  total_del ns_
	set sum 0
	set time 1.0
	set now [$ns_ now]

	for { set i 0} {$i <= $val(nn) } {incr i} { 
	set sum [expr $sum + [$dtn($i) set deleted_bundles]]
	}
	puts $total_del "$now $sum"
	$ns_ at [expr $now+$time] "record_total_deleted_packets"
}

#########################################################
# Calculation the average delivery delay
#########################################################

proc get_avg_delay {} {
	
	global val dtn avg_dl 
	set sum 0
	set n_dest 0
	for { set i 0} {$i <= $val(nn) } {incr i} {
        set t_d [$dtn($i) set  total_delay]
	set t_deli [$dtn($i) set  delivered_bundles]
	if { $t_deli > 0 } {
	set avg [expr $t_d / $t_deli]
	puts $avg_dl "$avg" 
	set sum [expr $sum + $avg]
	set n_dest [expr $n_dest + 1]
	}
	}
	if { $n_dest > 0 } {
	set avg_delay [expr $sum /$n_dest]
	puts $avg_dl "*******************************"
	puts $avg_dl "$avg_delay"
	}
}





#########################################################
# Checking a node for a given visitor
#########################################################


proc check_nodes_for_visitor { m_id {m_uid} } {
	global val dtn  ns_
	set nbr 0
	
	for { set i 0} {$i <= $val(nn) } {incr i} {
	$dtn($i) check_for_visitor $m_id $m_uid
	set exist [$dtn($i) set is_visitor_here]
	if { $exist == 1 } {
				set nbr [expr $nbr + 1]
			   }
	}
	return $nbr
}



#########################################################
# Tracking a given message
#########################################################


proc track_message_2 { m_id {m_uid} } {
	global val dtn  ns_  track_f
	set nbr 0

	for { set i 0} {$i <= $val(nn) } {incr i} {
	$dtn($i) check_for_message $m_id $m_uid
	set exist [$dtn($i) set is_message_here]
	if { $exist == 1 } {
				set nbr [expr $nbr + 1]
			   }
	}
	return $nbr
}

proc track_message {} {
	global val dtn  ns_  track_f
	set nbr_cp 0

	set tawa [$ns_ now]
	set next 5.0
	for { set i 0} {$i <= $val(nn) } {incr i} {
		$dtn($i) check_for_message "REGION1,_o15:0" "4"
		set exist [$dtn($i) set is_message_here]
		if { $exist == 1 } {
			set nbr_cp [expr $nbr_cp + 1]
		}
	}
	
	puts $track_f "$tawa $nbr_cp"

	$ns_ at [expr $tawa+$next] "track_message"

}

#$ns_ at 0.0 "track_message"


#########################################################
# Generating statistics
#########################################################

proc show_statistics {} {
	global val dtn td tdd generated_packets_tab avg_data_size avg_epidemic_data_size avg_stat_data_size
	set sum 0

	set tds 0 
	set ads 0	

	set teds 0 
	set aeds 0	

	set tsds 0 
	set asds 0
	
	set tperm 0
	set avg_perm 0 

	for { set i 0} {$i <= $val(nn) } {incr i} { 
	set deliv [$dtn($i) set delivered_bundles]
	set buffred [$dtn($i) set epidemic_buffered_bundles]
	set del [$dtn($i) set deleted_bundles]
	set delret [$dtn($i) set number_of_deleted_bundles_due_to_ret_failure]
	set reqbundle [$dtn($i) set number_of_asked_bundles]
	set recvbundles [$dtn($i) set number_recv_bundles]

        set epidemic_data [$dtn($i) set epidemic_control_data_size]
	set teds [ expr $teds + $epidemic_data]

	set stat_data [$dtn($i) set stat_data_size]	
	set tsds [expr $stat_data + $tsds]
	#set tsds [expr $tsds + $teds]

	set per_meeting_stat [$dtn($i) set avg_per_meeting_stat]	
	set tperm [expr $tperm + $per_meeting_stat]		

	set data [$dtn($i) set data_size]
	set tds [expr $tds + $data]

	#puts "node $i, delivered: $deliv , generated: $generated_packets_tab($i) , nbr buffred: $buffred ,nbr deleted policy: $del , deleted ret: $delret , requested: $reqbundle , recvbundles: $recvbundles"
	puts "Epidemic stat size: $epidemic_data Stat Data Size: $stat_data  Data Size: $data" 
	}
	
	set ads [expr $tds / $val(nn).0]
	set aeds [expr $teds / $val(nn).0]
	set asds [expr $tsds / $val(nn).0]
	set avg_perm [expr $tperm / $val(nn).0]

	puts $avg_data_size $ads 
	puts $avg_epidemic_data_size $aeds
	puts $avg_stat_data_size $asds
	puts $avg_stat_data_size $avg_perm
	 
}


proc finish {} {
	global td fg ns_  fd1  val dtn total_d total_del  avg_dl  dest_id avg_ld  track_f avg_data_size avg_epidemic_data_size avg_stat_data_size
	
	for { set i 0} {$i <= $val(nn) } {incr i} {
	$dtn($i) save_log
	}
	
	close $dest_id
	close $fg
	close $td
	close $total_d
	close $total_del
	close $avg_ld
	close $avg_dl
	close $track_f
	close $avg_data_size
	close $avg_epidemic_data_size
	close $avg_stat_data_size

	$ns_ halt
}

$ns_ at $sim_dur "puts \"End of simulation .\";generated_packets;delivered_packets;DeliveryRate;get_avg_delay;show_statistics;finish"


proc sim_elapsed_time {} {
	global ns_ sim_dur
	set time 100.0
	set now [$ns_ now]
	puts "										Simulation Duration Is $sim_dur ------ Current Simulation Time Is $now"
	$ns_ at [expr $now + $time] "sim_elapsed_time"
}
sim_elapsed_time

$ns_ run
