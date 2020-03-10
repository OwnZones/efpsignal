#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalReceive myEFPSignalReceive; //Needs to be destructed after the sender to avoid races in the tests.
bool testFail;

//If its ID 30 then set width and height
void declareContent(EFPStreamContent* content) {
  std::cout << "Declare content" << std::endl;
  if (content->mGStreamID == 30 && content->mGFrameContent == ElasticFrameContent::h264) {
    content->mVWidth = 1920;
    content->mVHeight = 1080;
  }
}

//Check if we got that info in the receiver.
void gotContentInformation(std::unique_ptr<EFPSignalReceive::EFPSignalReceiveData>& data) {
  std::cout << "Declare content reciever callback" << std::endl;
  for (auto &rItem: data->contentList) {
    if (rItem.mGStreamID == 30 && rItem.mVWidth != 1920 && rItem.mVHeight != 1080) {
      std::cout << "Content not as expected" << std::endl;
      testFail = true;
    }
  }
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

  EFPStreamContent thisEntry = myEFPSignalSend.getContent(ElasticFrameContent::h264,20);
  if (thisEntry.mGStreamID != 0) {
    return EXIT_FAILURE;
  }

  thisEntry = myEFPSignalSend.getContent(ElasticFrameContent::h264,30);
  if (thisEntry.mGStreamID != 30) {
    return EXIT_FAILURE;
  }

  if (thisEntry.mVWidth != 1920 || thisEntry.mVHeight != 1080) {
    return EXIT_FAILURE;
  }

  EFPStreamContent fakeContent(1500);
  fakeContent.mGFrameContent = ElasticFrameContent::adts;
  fakeContent.mGStreamID = 40;
  status = myEFPSignalSend.registerContent(fakeContent);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  sleep(1);
  json myStreamInfo = myEFPSignalSend.generateAllStreamInfo();
  std::cout << myStreamInfo.dump() << std::endl;
  sleep(6);

  std::cout << "Add fake" << std::endl;
  EFPStreamContent fakeContent2(1500);
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
  if (testFail) {
    return EXIT_FAILURE;
  }
  return 0;
}