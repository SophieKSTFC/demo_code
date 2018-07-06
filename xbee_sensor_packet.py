"""
@Author Sophie Kirkham STFC, 2018

"""

import datetime

""" xbee_sensor_packet 

represents packets sent from arduino xbee nodes 
"""
class xbee_sensor_packet():

    def __init__(self,  xbee_message):
        """ Cosntructs a sensor packet with default empty values

        Calls validate_packet() to populate values and check the contents are sane
        """
        self.message = xbee_message
        self.sender_address = ""
        self.cmd_type = ""
        self.timestamp = ""
        self.payload = {"detector" : 00, "temperature" : 00, "humidity" : 00}
        self.is_valid = False
        self.validate_packet()


    def validate_packet(self):
        """ Validate the contents of the packet as being sane
        Tries to pass all of the parameters, setting the member variables
        If the values cannot be passed, is_valid is set to false.
        """
        valid = True

        if self.message.remote_device.get_64bit_addr() is not None:
            self.sender_address = self.message.remote_device.get_64bit_addr()
        else: 
            valid = False
        if self.message.timestamp is not None:
            self.timestamp = datetime.datetime.fromtimestamp(self.message.timestamp).strftime('%Y-%m-%d %H:%M:%S')
        else:
            valid = False
        try:
            self.cmd_type = self.message.data[0]
        except IndexError:
            valid = False
        try:
            self.payload["detector"] = self.message.data[1]
        except IndexError:
            valid = False
        try:
            self.payload["temperature"] = self.message.data[2]
        except IndexError:
            valid = False
        try:
            self.payload["humidity"] = self.message.data[3]
        except IndexError:
            valid = False
        self.is_valid = valid

    def get_temperature(self):
        """ gets the temperature value of the packet if the packet is valid

        @returns :the temperature value in self.payload
        """
        if self.is_valid:
            return self.payload["temperature"]

    def get_detector_state(self):
        """ gets the detector status value of the packet if the packet is valid
        
        @returns :the detector value in self.payload
        """
        if self.is_valid:
            return self.payload["detector"]

    def get_humidity(self):
        """ gets the humidity value of the packet if the packet is valid
        
        @returns :the humidity value in self.payload
        """
        if self.is_valid:
            return self.payload["humidity"]

    def print_packet(self):
        """ Prints the content of the packet if the packet is valid
        """

        if self.is_valid:
            print("SENDER ADDRESS: %s." % self.sender_address)
            print("CMD TYPE: %s." % (format(self.cmd_type, '#04X')))
            print("PAYLOAD:\n")
            print("\tDETECTOR STATUS: %s." % format(self.get_detector_state(), '#04X'))
            print("\tTEMPERATURE: %s." % self.get_temperature())
            print("\tHUMIDITY: %s." % self.get_humidity())
            print("TIMESTAMP: %s.\n" % (self.timestamp))
