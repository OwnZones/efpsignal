#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalSend myEFPSignalSend(MTU, 5000);

void declareContent(EFPSignalSend::EFPStreamContent* content) {
  std::cout << "Callback working" << std::endl;
}

void sendData(const std::vector<uint8_t> &subPacket) {
  std::cout << "Send callback working" << std::endl;
}

int main() {
  std::cout << "Hello, World!" << std::endl;

  myEFPSignalSend.declareContentCallback = std::bind(&declareContent, std::placeholders::_1);
  myEFPSignalSend.sendCallback = std::bind(&sendData, std::placeholders::_1);
  std::vector<uint8_t>sendMe;
  sendMe.reserve(4000);
  myEFPSignalSend.signalPackAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),100,0);
  sleep(3);

  return 0;
}