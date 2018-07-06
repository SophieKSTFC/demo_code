"""
@Author Sophie Kirkham STFC, 2018

"""

from digi.xbee.devices import XBeeDevice
from digi.xbee.models.address import XBee64BitAddress, XBee16BitAddress
from digi.xbee.util import utils
import json
from time import sleep
import datetime


"""
coordinator class, extends XBeeDevice
Configures an xbee node to be used as a coordinator node
"""
class coordinator(XBeeDevice):

    def __init__(self, port, baud):
        """Initialise an coordinator object

        @param port: USB port to connect to
        @param baud: Baud rate to use

        Calls the superclass constructor
        """ 
        super(coordinator, self).__init__(port, baud)
        self.network = None
        self.devices = []

    def is_coordinator(self):
        """ Determine if the node is the coordinator

        @returns boolean: true if the node is the network coordinator
        """
        #get the coordinator enabled (CE) parameter from the device 
        CE = int.from_bytes(self.get_parameter("CE"), byteorder='big', signed=False)
        if(CE):
            return True
        else:
            return False

    def discover_network(self):
        """ Discovers the nodes network, sets self.network

        @returns the current network
        """
        self.network = self.get_network()
        return self.network

    def print_node_info(self):
        """ Print node information 
        """
        print("Node ID:                     %s" % (self.get_parameter("NI").decode()))
        print("PAN ID:                      %s" % (utils.hex_to_string(self.get_pan_id())))
        print("MAC Address:                 %s" % (utils.hex_to_string(self.get_64bit_addr())))
        print("Operating Mode:              %s" % (self.get_operating_mode()))
        print("Firmware Version:            %s" % (utils.hex_to_string(self.get_firmware_version())))
        print("Hardware Version:            %s" % (self.get_hardware_version()))
        print("Is Remote:                   %s" % (self.is_remote()))
        print("Is Network Co-ordinator:     %s" % (self.is_coordinator()))

    def get_mac_address(self):
        """ Return the mac address of the node

        @returns : string representation of the 64 bit address
        """
        return utils.hex_to_string(self.get_64bit_addr())

    def get_operating_mode(self):
        """ get the nodes operating mode

        Reads the AP parameter to see if the node is in Transparent Mode, 
        API Enabled or API Enabled with Escaping
        Uses a look up dictionary to return a string representation of the mode
        @returns : string value for the operating mode
        """
        mode = int.from_bytes(self.get_parameter("AP"), byteorder='big', signed=False)
        choices = {0: "T", 1: "API-1", 2: "API-2"}
        result = choices.get(mode, 'default')
        return result
        
    def get_node(self, node_id):
        """ Finds the device on the network with the given Node ID

        @param : node_id : the Node ID of the device to find

        Searches in the current network for devices with that name
        If there is no network, the coordinator is closed and system is shut
        If the node is not found, the coordinator is closed and system is shut

        @Returns : the node if found.
        """
        if self.network is None:
            print("Network has not been discovered..")
            self.close()
            exit(1)
        else:
            node = self.network.discover_device(node_id)
            if node is None:
                print("Node with ID %s could not be found..")
                self.close()
                exit(1)
            else: 
                print("Node '%s' located in network with MAC address:    %s" % (node_id, node.get_64bit_addr()))
            return node
