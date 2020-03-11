#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalReceive myEFPSignalReceive(5,2); //Needs to be destructed after the sender to avoid races in the tests.
bool testFail;

//If its ID 30 then set width and height
bool declareContent(EFPStreamContent* content) {
  std::cout << "Declare content" << std::endl;

  if (content->mVariables.mGStreamID == 30 && content->mVariables.mGFrameContent == ElasticFrameContent::h264) {
    content->mVariables.mVWidth = 1920;
    content->mVariables.mVHeight = 1080;
  }
  return true;
}

//Check if we got that info in the receiver.
void gotContentInformation(std::unique_ptr<EFPSignalReceive::EFPSignalReceiveData>& rData) {
  std::cout << "Declare content reciever callback" << std::endl;
  for (auto &rItem: rData->mContentList) {
    if (rItem.mVariables.mGStreamID == 30 && rItem.mVariables.mVWidth != 1920 && rItem.mVariables.mVHeight != 1080) {
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
  myEFPSignalSend.mEmbedBinary = false;
  myEFPSignalSend.mEmbedOnlyChanges = false;

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

  //This should not time out we will refresh later.
  status = myEFPSignalSend.packAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),31,0);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

  EFPStreamContent thisEntry(UINT32_MAX);
  status = myEFPSignalSend.getContent(thisEntry,ElasticFrameContent::h264,20);
  if (status != ElasticFrameMessages::contentNotListed) {
    return EXIT_FAILURE;
  }

  status = myEFPSignalSend.getContent(thisEntry,ElasticFrameContent::h264,30);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

  if (thisEntry.mVariables.mGStreamID != 30) {
    return EXIT_FAILURE;
  }

  if (thisEntry.mVariables.mVWidth != 1920 || thisEntry.mVariables.mVHeight != 1080) {
    return EXIT_FAILURE;
  }

  EFPStreamContent fakeContent(1500);
  fakeContent.mVariables.mGFrameContent = ElasticFrameContent::adts;
  fakeContent.mVariables.mGStreamID = 40;
  status = myEFPSignalSend.registerContent(fakeContent);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  json myStreamInfo;
  status = myEFPSignalSend.generateAllStreamInfoJSON(myStreamInfo);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  std::cout << myStreamInfo.dump() << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  myEFPSignalSend.mEmbedBinary = true;

  //This is the refresh.
  status = myEFPSignalSend.packAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),31,0);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

  std::this_thread::sleep_for(std::chrono::seconds(3));

  //This is the refresh.
  status = myEFPSignalSend.packAndSend(sendMe,ElasticFrameContent::h264,100,100,EFP_CODE('A','N','X','B'),31,0);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "Add fake" << std::endl;
  EFPStreamContent fakeContent2(1500);
  fakeContent2.mVariables.mGFrameContent = ElasticFrameContent::adts;
  fakeContent2.mVariables.mGStreamID = 0;
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::reservedStreamValue) {
    return EXIT_FAILURE;
  }
  fakeContent2.mVariables.mGStreamID = 60;
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  status =  myEFPSignalSend.registerContent(fakeContent2);
  if (status != ElasticFrameMessages::contentAlreadyListed) {
    return EXIT_FAILURE;
  }

  std::cout << "del fake" << std::endl;
  status = myEFPSignalSend.deleteContent(fakeContent2.mVariables.mGFrameContent,fakeContent2.mVariables.mGStreamID);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }
  status = myEFPSignalSend.deleteContent(fakeContent2.mVariables.mGFrameContent,fakeContent2.mVariables.mGStreamID);
  if (status != ElasticFrameMessages::contentNotListed) {
    return EXIT_FAILURE;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  status = myEFPSignalSend.generateAllStreamInfoJSON(myStreamInfo);
  if (status != ElasticFrameMessages::noError) {
    return EXIT_FAILURE;
  }

  std::cout << myStreamInfo.dump() << std::endl;
  if (testFail) {
    return EXIT_FAILURE;
  }
  return 0;
}