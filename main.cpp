#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalSend myEFPSignalSend(MTU, 5000);

void declareContent(EFPSignalSend::EFPStreamContent* content) {
  std::cout << "Declare content working" << std::endl;
}

void sendData(const std::vector<uint8_t> &subPacket) {
  std::cout << "Send callback working" << std::endl;
}

int main() {
  std::cout << "EFPSignal tests" << std::endl;

  ElasticFrameMessages status;

  myEFPSignalSend.declareContentCallback = std::bind(&declareContent, std::placeholders::_1);
  myEFPSignalSend.sendCallback = std::bind(&sendData, std::placeholders::_1);
  std::vector<uint8_t>sendMe;
  sendMe.reserve(4000);
  myEFPSignalSend.packAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),100,0);

  EFPSignalSend::EFPStreamContent fakeContent(1500);
  fakeContent.mGFrameContent = ElasticFrameContent::adts;
  fakeContent.mGStreamID = 40;
  myEFPSignalSend.registerContent(fakeContent);

  sleep(1);
  json myStreamInfo = myEFPSignalSend.generateAllStreamInfo();
  std::cout << myStreamInfo.dump() << std::endl;
  sleep(6);

  std::cout << "Add fake" << std::endl;
  EFPSignalSend::EFPStreamContent fakeContent2(1500);
  fakeContent2.mGFrameContent = ElasticFrameContent::adts;
  fakeContent2.mGStreamID = 0;
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::reservedStreamValue) {
    return EXIT_FAILURE;
  }
  fakeContent2.mGStreamID = 60;
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::contentAlreadyListed) {
    return EXIT_FAILURE;
  }

  std::cout << "del fake" << std::endl;
  status = myEFPSignalSend.deleteContent(fakeContent2.mGFrameContent,fakeContent2.mGStreamID);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  status = myEFPSignalSend.deleteContent(fakeContent2.mGFrameContent,fakeContent2.mGStreamID);
  if (status != ElasticFrameMessages::contentNotListed) {
    return EXIT_FAILURE;
  }

  sleep(1);
  myStreamInfo = myEFPSignalSend.generateAllStreamInfo();
  std::cout << myStreamInfo.dump() << std::endl;
  return 0;
}