#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalReceive myEFPSignalReceive(5,2); //Needs to be destructed after the sender to avoid races in the tests.
bool testFail;

//If its ID 30 then set width and height
bool declareContent(EFPStreamContent* content) {
  return true;
}

//Check if we got that info in the receiver.
void gotContentInformation(std::unique_ptr<EFPSignalReceive::EFPSignalReceiveData>& data) {

}

//Normal EFP send
void sendData(const std::vector<uint8_t> &subPacket, uint8_t streamID) {
  myEFPSignalReceive.receiveFragment(subPacket,0);
}

//Normal EFP Receive
void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
  std::cout << "gotData" << std::endl;
}

int main() {
  testFail = false;

  EFPSignalSend myEFPSignalSend(MTU, 5000);

  std::cout << "EFPSignalSend protocol version " << unsigned(myEFPSignalSend.signalVersion()) << std::endl;
  std::cout << "EFPSignalReceive protocol version " << unsigned(myEFPSignalReceive.signalVersion()) << std::endl;

  myEFPSignalSend.mEmbedInterval100msSteps = 10;

  ElasticFrameMessages status;

  myEFPSignalSend.declareContentCallback = std::bind(&declareContent, std::placeholders::_1);
  myEFPSignalSend.sendCallback = std::bind(&sendData, std::placeholders::_1, std::placeholders::_2);
  myEFPSignalReceive.receiveCallback = std::bind(&gotData, std::placeholders::_1);
  myEFPSignalReceive.contentInformationCallback = std::bind(&gotContentInformation, std::placeholders::_1);

  std::vector<uint8_t>sendMe(4000);

  status = myEFPSignalSend.packAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),30,0);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

 auto myData = myEFPSignalSend.generateAllStreamInfoData();
  std::vector<uint8_t> *test = myData->;

  return 0;
}