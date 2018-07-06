#include <Vector.h>
#include <AltSoftSerial.h>
#include <Printers.h>
#include <XBee.h>
#include <stdio.h> // for function sprintf
#include <stdlib.h>
#include <string.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

//CMD definitions
#define CMD_READ 0x01
#define CMD_ACK 0x05
#define CMD_REPLY 0x02
#define CMD_SHUTDOWN 0xFF
#define CMD_POWERON 0xEE
#define CMD_ERROR 0x55
#define LED 13
#define DETECTOR_SHUTDOWN 0xD0
#define DETECTOR_OPERATIONAL 0xD1

//Parameter values for the XBee configuration
uint8_t SH[] = {'S','H'};
uint8_t SL[] = {'S','L'};
uint8_t AP[] = {'A','P'};
uint8_t VR[] = {'V','R'};
uint8_t NI[] = {'N','I'};
uint8_t ID[] = {'I','D'};
uint8_t CE[] = {'C','E'};
uint8_t HV[] = {'H','V'};

//Dummy detector to turn on and off when the environmental conditions are bad
struct i2c_detector{

    uint8_t state = DETECTOR_OPERATIONAL;
};


struct xbee_packet{

  XBeeAddress64 sender_address;
  uint8_t cmd_type;
  Vector<uint8_t> data;
  uint8_t packet_length;
  uint8_t data_length;

  void print_packet(){

      DebugSerial.print(F("Command Type: 0x"));
      DebugSerial.println(this->cmd_type, HEX);
      DebugSerial.print(F("Payload: "));
      for(int i = 0; i < this->data.Size(); i++){
            DebugSerial.print("0x");
            DebugSerial.print(this->data[i], HEX);
            DebugSerial.print(", ");
      }
      DebugSerial.println();
      DebugSerial.print(F("Packet Length: 0x"));
      DebugSerial.print(this->packet_length);
      DebugSerial.println();
      DebugSerial.println(F("------------------------"));
 }
};


void print_ascii(uint8_t value){
  DebugSerial.write(value);
}

void print_hex(uint8_t value){
  DebugSerial.print(value, HEX);
}

void print_bool(uint8_t value){
  if(value){
    DebugSerial.print(F("True"));
  }
  else{
    DebugSerial.print(F("False"));
  }
}


class xbee_node : public XBee{

  public:
  
        XBeeAddress64 coordinator_address;
        bool rx_ack;
        bool sleep;

    /*
    Prints all the parameter information about the xbee node
    */    
    void print_node_info(){

        DebugSerial.print(F("Node ID:               "));
        print_param(NI,'A');
        DebugSerial.println("");
        DebugSerial.print(F("PAN ID:                "));
        print_param(ID,'H');
        DebugSerial.println("");
        DebugSerial.print(F("MAC Address:           "));
        print_param(SH,'H');
        print_param(SL,'H');
        DebugSerial.println("");
        DebugSerial.print(F("Operating Mode:        API-"));
        print_param(AP,'H');
        DebugSerial.println("");
        DebugSerial.print(F("Firmware Version:      "));
        print_param(VR,'H');
        DebugSerial.println("");
        DebugSerial.print(F("Hardware Version:      "));
        print_param(HV,'H');
        DebugSerial.println("");
        DebugSerial.print(F("Is Co-ordinator:       "));
        print_param(CE,'B');
        DebugSerial.println("");

    }
     /*
    Sends a message to an xbee node

    @param node_address: reference to an XBeeAddress to send packet to
    @param packet : reference to the packet we want to send
    */ 
    void send_msg(XBeeAddress64 &node_address, xbee_packet &packet){

      // construct the payload from the packet content
      uint8_t payload[packet.packet_length];
      payload[0] = packet.cmd_type;
      for(int i = 0; i < packet.data_length; i++){
        payload[i+1] = packet.data[i];  
      }

      //create and send a zigbee TX request
      ZBTxRequest zbTx = ZBTxRequest(node_address, payload, sizeof(payload));
      this->send(zbTx);
      
      // check the status (was it acknowledges and received)
      ZBTxStatusResponse txStatus = ZBTxStatusResponse();
      
      if(this->readPacket(5000)){
        if (this->getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
        
            this->getResponse().getZBTxStatusResponse(txStatus);
        
            if (txStatus.getDeliveryStatus() == SUCCESS) {
              // success.  time to celebrate
              DebugSerial.println(F("Packet Received"));
            } else {
              // the remote XBee did not receive our packet. is it powered on?
               DebugSerial.println(F("Packet Not Received"));
            }
        }
      }
    }

    /*
    Checks whether a node at the address is the coordinator for the network

    @param node_address: reference to an XBeeAddress to check
    @returns : true or false depedning on if the node is the coordinator
    */ 
    bool is_remote_coordinator(XBeeAddress64 &node_address){

      //send a remote AT request to the device to read the CE param
      uint8_t value;
      RemoteAtCommandRequest remt_req = RemoteAtCommandRequest(node_address, CE);
      RemoteAtCommandResponse remt_rep = RemoteAtCommandResponse();
      this->send(remt_req);

      // read the response and get the parameter value if the resposne is OK
      if (this->readPacket(5000)){
        if (this->getResponse().getApiId() == REMOTE_AT_COMMAND_RESPONSE){
          
          this->getResponse().getRemoteAtCommandResponse(remt_rep);
          if (remt_rep.isOk()){
            if (remt_rep.getValueLength() > 0){
              for (int i = 0; i < remt_rep.getValueLength(); i++){
                value = remt_rep.getValue()[i];
              }
              DebugSerial.println("");
            }
          }
          else{
              DebugSerial.print("Command returned error code: ");
              DebugSerial.println(remt_rep.getStatus(), HEX);
          }
        } 
        else{
            DebugSerial.print("Expected Remote AT response but got ");
            DebugSerial.print(this->getResponse().getApiId(), HEX);
        }    
      } 
      else if (this->getResponse().isError()){
          DebugSerial.print("Error reading packet.  Error code: ");  
          DebugSerial.println(this->getResponse().getErrorCode());
      }
      else{
          DebugSerial.print("No response from radio");  
      }
      return (value ? true : false); 
    }

    /*
    Receives a message from an xbee node

    @param packet : reference to the packet we want to receive into
    */ 
    void recv_msg(xbee_packet &packet){
      
      
      this->rx_ack = false;
      XBeeAddress64 node_address;
      XBeeResponse response = XBeeResponse();
      ZBRxResponse rx = ZBRxResponse();

      // get the message
      this->readPacket();
      // if theres a response and it is a RX request
       if (this->getResponse().isAvailable()) {
        
          if(this->getResponse().getApiId() == ZB_RX_RESPONSE){
            
              DebugSerial.println(F("------------------------"));
              DebugSerial.print("Received Zigbee RX Request From: ");
              this->getResponse().getZBRxResponse(rx);

              //pass all of the information into our packet
              node_address = rx.getRemoteAddress64();
              packet.sender_address = node_address;
              printHex(DebugSerial, node_address);
              DebugSerial.println();

              packet.cmd_type = rx.getData(0);
              packet.packet_length =  rx.getDataLength();
              for(int i = 1; i < rx.getDataLength(); i++){
                packet.data.PushBack(rx.getData(i));
              }
              // print the packet
              packet.print_packet();
              //double check it was acknowledged by them
              if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
                  this->rx_ack = true;
              }
          }
        }
       else if (this->getResponse().isError()) {
          DebugSerial.print("##### ----- Error reading packet.  Error code: ");  
          DebugSerial.println(this->getResponse().getErrorCode());
      }

    }
    /*
    Prints a parameter value read from a remote XBee device

    @param param: the parameter to read
    @param type : the type of parameter (Hex, Ascii, Boolean)
    */
    void print_param(uint8_t param[], char type){

      AtCommandRequest atRequest = AtCommandRequest(param);
      AtCommandResponse atResponse = AtCommandResponse();
    
      this->send(atRequest);
      
      if(this->readPacket(5000))
      {
          if(this->getResponse().getApiId()== AT_COMMAND_RESPONSE)
          {
            this->getResponse().getAtCommandResponse(atResponse);
            if(atResponse.isOk())
            {
              if(atResponse.getValueLength() > 0)
              {

                for (int i = 0; i < atResponse.getValueLength(); i++)
                {

                  switch(type){
                    case 'H':
                      print_hex(atResponse.getValue()[i]);
                      break; 
                    case 'A':
                      print_ascii(atResponse.getValue()[i]); 
                      break;
                    case 'B':
                      print_bool(atResponse.getValue()[i]);
                      break;
                    default:
                      break;
                  }
                }
              }
            }
            else
            {
              DebugSerial.print(F("Error occured during retrieving parameter: "));
              DebugSerial.println(atResponse.getStatus(), HEX);
            }
            
          }
          else{
            DebugSerial.print(F("Did not get an AT COMMAND Response."));
          }
       }
    }

};// xbee_node


//----------------------------//

xbee_node router; 
Adafruit_BME280 bme; 
i2c_detector detector;
unsigned long packet_count = 0;
bool is_coord;

void setup() {

  pinMode(LED, OUTPUT); // set the LED to be an output pin
  //set up the serial connections
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));
  XBeeSerial.begin(9600);
  router.begin(XBeeSerial);
  delay(1);

  // print the xbee node info
  router.print_node_info();

  //flash an led 
  digitalWrite(LED, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);

  // check the sensor is connected and up and running!
  bool status;
  status = bme.begin();  
  if (!status) {
      DebugSerial.println("Could not find a valid BME280 sensor, check wiring!");
      while (1);
  }
  else{
    DebugSerial.println("BME280 Sensor Configured");
  }
  
}

// loop over this indefinitely 
void loop() {

    xbee_packet request_message;
    router.recv_msg(request_message);// receive a message

    // if we actually received something and the sender received an ack..
    if(router.rx_ack){ 
      
      // double check that the node sending this is a co-ordinator, but only do this once..!
      if (packet_count == 0){ 
        is_coord = router.is_remote_coordinator(request_message.sender_address);
      }
      
      packet_count++;

      //if the node was the coordinator of the network handle the request
      if(is_coord){
        
        xbee_packet reply_packet;
        uint8_t i2c_temp;
        uint8_t i2c_humidity;
        
        // if the request is a read request, read data!
        if (request_message.cmd_type == CMD_READ){
          DebugSerial.println(F("##### ----- Received READ Packet.. Reading. ----- #####"));
          //populate read packet and read from the sensor
          reply_packet.cmd_type = CMD_REPLY;
          i2c_temp = bme.readTemperature();
          i2c_humidity = bme.readHumidity();
          reply_packet.data.PushBack(detector.state);
          reply_packet.data.PushBack(i2c_temp);
          reply_packet.data.PushBack(i2c_humidity);
          reply_packet.packet_length =  reply_packet.data.Size() + 1;
          reply_packet.data_length =  reply_packet.data.Size();
          //send the message!
          router.send_msg(router.coordinator_address, reply_packet);
        }

        // If the request is just an acknowledgement, do nothing
        else if(request_message.cmd_type == CMD_ACK){
          DebugSerial.println(F("##### ----- Received ACK Packet.. Doing Nothing ----- #####"));
          
        }
        // If the request is to power off, power off!
        else if(request_message.cmd_type == CMD_SHUTDOWN){
            DebugSerial.println(F("##### ----- Received Shut Down ----- #####"));
            power_down_detector();
   
        }
        // If the request is to power on, power on!
        else if(request_message.cmd_type == CMD_POWERON){
            DebugSerial.println(F("##### ----- Received Power On ----- #####"));
            power_on_detector();
        }
        // let the user know the last packet was not correct but ignore it
        else if(request_message.cmd_type == CMD_ERROR){
            DebugSerial.println(F("##### ----- LAST PACKET WAS FAULTY ----- #####"));
        }
      }
    // it wasnt the coordinator sending me a packet, ignore it
    else{
      DebugSerial.println("I Only Listen To My Co-ordinator");
    }

  }// not ack'd
   request_message.cmd_type = 0x00;
}

 /*Dummy method to power off the detector
 Sets the detector state to SHUTDOWN
 Turns the LED ON
 */
void power_down_detector(){
  detector.state = DETECTOR_SHUTDOWN;
  DebugSerial.println(F("Powering Down the Detector"));
  digitalWrite(LED, HIGH); 

 }
 
 /*Dummy method to power on the detector
 Sets the detector state to OPERATIONAL
 Turns the LED OFF
 */
 void power_on_detector(){
  detector.state = DETECTOR_OPERATIONAL;
  DebugSerial.println(F("Powering On the Detector"));
  digitalWrite(LED, LOW);
 
 }

