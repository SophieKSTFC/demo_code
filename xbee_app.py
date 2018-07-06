"""
@Author Sophie Kirkham STFC, 2018

"""
from digi.xbee.devices import XBeeDevice
from digi.xbee.models.address import XBee64BitAddress, XBee16BitAddress
from digi.xbee.util import utils
from time import sleep
import datetime
import sys
from coordinator import coordinator
from xbee_sensor_packet import xbee_sensor_packet

#CMD Definitions 
CMD_POLL = 0x01
CMD_REPLY = 0x02
CMD_ACK = 0x05
CMD_SHUTDOWN = 0xFF
CMD_ERROR = 0x55
CMD_POWERON = 0xEE
DETECTOR_SHUTDOWN = 0xD0
DETECTOR_OPERATIONAL = 0xD1
MAX_TEMP = 32

#adjust as appropriate for the configuration
BAUD_RATE = 9600
MAC_COORD_PORT = "/dev/tty.usbserial-DN01J874"
WIN_COORD_PORT = "COM"
PAN_ID = 0x4444
COORD_MAC = 0x0013A20041531EE7
ROUT_MAC = 0x0013A20041052F50
ROUTER_ID = "end-node"
router_64addr = XBee64BitAddress.from_hex_string("0013A20041052F50")

# main entry point for xbee_app
def main():

    coord = coordinator(MAC_COORD_PORT, BAUD_RATE) #coordinator node

    # set up variables for tracking packets and temperature
    average_temp = 0
    read_count = 0
    total_temp = 0 
    temperature_readings = []
    packet_num = 0   

    # open the node, print it's information and connect the router node
    print("Opening Device at Port %s at %s baud rate." % (MAC_COORD_PORT, BAUD_RATE))
    coord.open()
    coord.print_node_info()
    coord.discover_network()
    router = coord.get_node(ROUTER_ID)

    # populate the list of network devices, note - this is dependant on get_node()
    network_devices = coord.network.get_devices()

    # print the node addresses in the network
    print("\nNetwork Devices Discovered: ")
    for node in network_devices:
        print("Node %s" % node.get_64bit_addr())

    # loop until we press control c        
    while True:

        try:
            # send a "poll data" request to every node
            print("Sending Request To Start Poll Data .. \n")
            req_packet = bytearray([CMD_POLL])

            # this system only works with one node, needs adjusting for multiple
            for node in network_devices:
                coord._send_data_64(node.get_64bit_addr(), req_packet)

            # check if there are any packets waiting to be parsed 
            if coord.has_packets():
                xbee_message = coord.read_data(5) #need the delay to synchronise
            
            # as long as the message isnt empty.. 
            if xbee_message is not None:
                
                # build sensor_packet out of the message
                sensor_packet = xbee_sensor_packet(xbee_message)

                # check the packet is valid, else quit
                if not sensor_packet.is_valid:
                    print("Received Invalid XBee Packet")
                    coord.close()
                    exit(1)

                # print the packet out 
                print("Received Response\n")
                sensor_packet.print_packet()

                # if it was a reply packet
                if sensor_packet.cmd_type == CMD_REPLY:
                    
                    # pass the payload values
                    temp = sensor_packet.get_temperature()
                    humidity = sensor_packet.get_humidity()
                    detector_state = sensor_packet.get_detector_state()
                    temperature_readings.append(temp)

                    print("Temperature, Humidity and Detector Status: ")
                    print("Current Temperature: %d." % (temp))
                    print("Current Humidity: %d." % (humidity))

                    if detector_state is DETECTOR_OPERATIONAL:
                        print("Detector is currently : OPERATIONAL")
                    else:
                        print("Detector is currently : SHUT DOWN")
                    
                    # calculate average temperature so far
                    read_count += 1
                    
                    for temp in temperature_readings:
                        total_temp += temp

                    average_temp = total_temp/read_count
                    print("Average Temperature: %d\n" % average_temp)

                    total_temp = 0 # reset this value

                    """
                    if the temperature is above the threshold of safety, 
                    and the detector is currently operating, shut it down.
                    
                    else if the temperature is below the threshold of safety, 
                    and the detector is currently powered off, turn it back on.
                    
                    """
                    if temp > MAX_TEMP and detector_state is DETECTOR_OPERATIONAL:
                        print("\n\n####### TEMPERATURE CRITICAL #######\n\n")
                        reply_packet = bytearray([CMD_SHUTDOWN])

                    elif temp < MAX_TEMP and detector_state is DETECTOR_SHUTDOWN:
                        print("Powering back on detector")
                        reply_packet = bytearray([CMD_POWERON])

                    #otherwise, just acknowledge the packet, everythings fine!
                    else:
                        reply_packet = bytearray([CMD_ACK])
                else:
                    # if we didnt get a reply packet, notify the arduino
                    print("Unrecognised Packet Type")
                    reply_packet = bytearray([CMD_ERROR])

                # send the arduino the packet
                coord._send_data_64(router_64addr, reply_packet)
                packet_num +=1

                print("Packet Count: %d.\n" % packet_num)
                print("---------------------------------")

        # quits the program if control C is pressed
        except KeyboardInterrupt:
            coord.close()
            break


if __name__ == "__main__":
    main()