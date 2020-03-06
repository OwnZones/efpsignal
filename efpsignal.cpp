//
// Created by Anders Cedronius on 2020-03-06.
//

#include "efpsignal.h"
#include "efpsignalinternal.h"

EFPSignalSend::~EFPSignalSend() {
  LOGGER(true, LOGG_NOTIFY, "EFPSignal destructed")
  mThreadRunSignal = false;
  uint32_t lLockProtect = 1000;
  //check for it to actually stop
  while (mSignalThreadActive) {
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    if (!--lLockProtect) {
      //we gave it a second now exit anyway
      LOGGER(true, LOGG_FATAL, "Threads not stopping. Now crash and burn baby!!")
      break;
    }
  }
}

ElasticFrameMessages
EFPSignalSend::SignalPackAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                  uint32_t code,
                  uint8_t streamID, uint8_t flags) {

  if (!isKnown[dataContent][streamID]) {
    if (mDropUnknown) {
      return ElasticFrameMessages::efpSignalDropped;
    }
    EFPStreamContent newContent(mGarbageCollectms);
    newContent.mGStreamID = streamID;
    newContent.mGFrameContent = dataContent;
    if (declareContentCallback) {
      declareContentCallback(&newContent);
    }
    efpStreamLists[streamID].push_back(newContent);
    isKnown[dataContent][streamID] = true;
    mNewEntry = true;
  }

  return this->packAndSend(rPacket, dataContent, pts, dts, code, streamID, flags);
}

ElasticFrameMessages
EFPSignalSend::SignalPackAndSendFromPtr(const uint8_t* pPacket, size_t packetSize, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                         uint32_t code, uint8_t streamID, uint8_t flags) {

  if (!isKnown[dataContent][streamID]) {
    isKnown[dataContent][streamID] = true;
    mNewEntry = true;
  }

  return this->packAndSendFromPtr(pPacket, packetSize, dataContent, pts, dts, code, streamID, flags);
}

void EFPSignalSend::signalWorker() {
  mSignalThreadActive = true;
  while (mThreadRunSignal) {
    //Run signal worker 10 times per second
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (auto &rGroup: efpStreamLists) {
      for (auto &rItems: rGroup) {
        if(rItems.isStillAlive(100)) {

        }
      }
    }
  }
  mSignalThreadActive = false;
}



void EFPSignalSend::startSignalWorker() {
  std::thread(std::bind(&EFPSignalSend::signalWorker, this)).detach();
}