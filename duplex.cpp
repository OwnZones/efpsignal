#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

// ---------------------------------------------------------------------------------------------------------------
//
//
// Sender
//
//
// ---------------------------------------------------------------------------------------------------------------

std::atomic_bool runThreads;
std::atomic_int threadsActive;
EFPSignalDuplex myEFPDuplexGenerator(5,2,MTU,5000);
EFPSignalDuplex myEFPDuplexReceiver(5,2,MTU,5000);
bool hasGotWhatIWantYet;

void generateContent(ElasticFrameContent content, uint8_t streamID, uint32_t code, double bitRate, uint32_t fps) {
  threadsActive++;
  double bytesPerSecond = (( bitRate / 8.0 ) + 0.5);
  uint64_t frameSize = bytesPerSecond / fps;
  std::vector<uint8_t> myData(frameSize);
  std::generate(myData.begin(), myData.end(), [n = 0]() mutable { return n++; });
  uint64_t ptsDistance = 90000 / fps;
  uint64_t ptsCounter = 0;
  while (runThreads) {
    myEFPDuplexGenerator.mEFPSend -> packAndSend(myData, content, ptsCounter, ptsCounter, code, streamID, NO_FLAGS);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000/fps));
    ptsCounter += ptsDistance;
  }
  threadsActive--;
}

void startSignalGenerators() {
  std::thread(std::bind(&generateContent, ElasticFrameContent::adts, 30, EFP_CODE('A','D','T','S'), 128000, 48)).detach();
  std::thread(std::bind(&generateContent, ElasticFrameContent::h264, 31, EFP_CODE('A','N','X','B'), 4000000, 50)).detach();
  std::thread(std::bind(&generateContent, ElasticFrameContent::h264, 32, EFP_CODE('A','N','X','B'), 2000000, 50)).detach();
  std::thread(std::bind(&generateContent, ElasticFrameContent::h264, 33, EFP_CODE('A','N','X','B'), 1000000, 50)).detach();
}

bool declareContentGen(EFPStreamContent& content) {
  std::cout << "Declare content generator:" << unsigned(content.mVariables.mGStreamID) << std::endl;

  if (content.mVariables.mGStreamID == 31) {
    content.mVariables.mVBitsPerSec = 4000000;
  } else if (content.mVariables.mGStreamID == 32) {
    content.mVariables.mVBitsPerSec = 2000000;
  } else if (content.mVariables.mGStreamID == 33) {
    content.mVariables.mVBitsPerSec = 1000000;
  }
  return false;
}

void sendDataGen(const std::vector<uint8_t> &subPacket, uint8_t streamID) {
  if (myEFPDuplexReceiver.mEFPReceive) {
    ElasticFrameMessages status = myEFPDuplexReceiver.mEFPReceive->receiveFragment(subPacket, 0);
    if (status != ElasticFrameMessages::noError) {
      std::cout << "Error receiving data" << std::endl;
    }
  }
}

void gotDataGen(ElasticFrameProtocolReceiver::pFramePtr &packet) {
  std::cout << "G" << std::endl;
}

// ---------------------------------------------------------------------------------------------------------------
//
//
// Receiver
//
//
// ---------------------------------------------------------------------------------------------------------------

bool declareContentRcv(EFPStreamContent& content) {
  std::cout << "Declare content receiver:" << unsigned(content.mVariables.mGStreamID) << std::endl;
  return true;
}

void gotContentInformationRcv(std::unique_ptr<EFPSignalReceive::EFPSignalReceiveData>& rData) {

  if (!hasGotWhatIWantYet) {
    for (auto &rItem: rData->mContentList) {
      if (rItem.mVariables.mGFrameContent == ElasticFrameContent::h264 && rItem.mVariables.mGStreamID == 31) {
        EFPSignalMessages::WhiteList myWhiteListItem;
        myWhiteListItem.mStreamID = rItem.mVariables.mGStreamID;
        myWhiteListItem.mContent = rItem.mVariables.mGFrameContent;
        myEFPDuplexReceiver.mEFPSend->packAndSendFromPtr((uint8_t *) &myWhiteListItem,
                                                         sizeof(EFPSignalMessages::WhiteList),
                                                         ElasticFrameContent::efpsig,
                                                         0,
                                                         0,
                                                         EFP_CODE('D', 'M', 'S', 'G'),
                                                         0,
                                                         0);
        std::cout << "Found my target " << std::endl;
      }
    }
  }
}

void sendDataRcv(const std::vector<uint8_t> &subPacket, uint8_t streamID) {
  ElasticFrameMessages status = myEFPDuplexGenerator.mEFPReceive -> receiveFragment(subPacket,0);
  if (status != ElasticFrameMessages::noError) {
    std::cout << "Error sending data" << std::endl;
  }
}

void gotDataRcv(ElasticFrameProtocolReceiver::pFramePtr &packet) {
  if (packet->mStreamID == 31 && packet->mDataContent == ElasticFrameContent::h264) {
    hasGotWhatIWantYet = true;
    std::cout << "Yey.. got my video" << std::endl;
  } else {
    std::cout << "Got things I did not request" << std::endl;
  }
}

int main() {
  std::cout << "Duplex tests running" << std::endl;

  hasGotWhatIWantYet = false;

  //Declare callbacks generator
  myEFPDuplexGenerator.mEFPSend -> declareContentCallback = std::bind(&declareContentGen, std::placeholders::_1);
  myEFPDuplexGenerator.mEFPSend -> sendCallback = std::bind(&sendDataGen, std::placeholders::_1, std::placeholders::_2);
  myEFPDuplexGenerator.mEFPReceive -> receiveCallback = std::bind(&gotDataGen, std::placeholders::_1);

  //Declare callbacks receiver
  myEFPDuplexReceiver.mEFPSend->declareContentCallback = std::bind(&declareContentRcv, std::placeholders::_1);
  myEFPDuplexReceiver.mEFPSend->sendCallback = std::bind(&sendDataRcv, std::placeholders::_1, std::placeholders::_2);
  myEFPDuplexReceiver.mEFPSend->mAutoRegister = false;
  myEFPDuplexReceiver.mEFPReceive->receiveCallback = std::bind(&gotDataRcv, std::placeholders::_1);
  myEFPDuplexReceiver.mEFPReceive->contentInformationCallback = std::bind(&gotContentInformationRcv, std::placeholders::_1);

  myEFPDuplexReceiver.mEFPSend->mEmbedInStream = false;

  runThreads = true;
  startSignalGenerators();
  std::this_thread::sleep_for(std::chrono::seconds(5));

  runThreads = false;
  int forceQuit = 100;
  while (threadsActive) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!forceQuit--) {
      std::cout << "Force quit with threads running. " << std::endl;
      break;
    }
  }

  std::cout << "Duplex tests exit" << std::endl;
  return 0;
}