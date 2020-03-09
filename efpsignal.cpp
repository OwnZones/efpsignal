//
// Created by Anders Cedronius on 2020-03-06.
//

#include "efpsignal.h"
#include "efpsignalinternal.h"

EFPSignalSend::~EFPSignalSend() {
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
  LOGGER(true, LOGG_NOTIFY, "EFPSignal destruct")
}

ElasticFrameMessages EFPSignalSend::signalFilter(ElasticFrameContent dataContent, uint8_t streamID) {
  if (!isKnown[streamID][dataContent]) {
    if (mDropUnknown) {
      return ElasticFrameMessages::efpSignalDropped;
    }
    if (autoRegister) {

      EFPStreamContent newContent(mGarbageCollectms);
      newContent.mGStreamID = streamID;
      newContent.mGFrameContent = dataContent;
      if (declareContentCallback) {
        declareContentCallback(&newContent);
      }
      std::lock_guard<std::mutex> lock(mStreamListMtx);
      efpStreamLists[streamID].push_back(newContent);
      isKnown[streamID][dataContent] = true;
      LOGGER(true, LOGG_NOTIFY, "Added entry")
      mStreamListChanged = true;
    }
  }
  return ElasticFrameMessages::noError;
}


ElasticFrameMessages
EFPSignalSend::packAndSend(const std::vector<uint8_t> &rPacket,
                           ElasticFrameContent dataContent,
                           uint64_t pts,
                           uint64_t dts,
                           uint32_t code,
                           uint8_t streamID,
                           uint8_t flags) {

  ElasticFrameMessages signalMess = signalFilter(dataContent, streamID);
  if (signalMess != ElasticFrameMessages::noError) {
    return signalMess;
  }
  return ElasticFrameProtocolSender::packAndSend(rPacket, dataContent, pts, dts, code, streamID, flags);
}

ElasticFrameMessages
EFPSignalSend::packAndSendFromPtr(const uint8_t *pPacket,
                                  size_t packetSize,
                                  ElasticFrameContent dataContent,
                                  uint64_t pts,
                                  uint64_t dts,
                                  uint32_t code,
                                  uint8_t streamID,
                                  uint8_t flags) {
  ElasticFrameMessages signalMess = signalFilter(dataContent, streamID);
  if (signalMess != ElasticFrameMessages::noError) {
    return signalMess;
  }
  return ElasticFrameProtocolSender::packAndSendFromPtr(pPacket,
                                                        packetSize,
                                                        dataContent,
                                                        pts,
                                                        dts,
                                                        code,
                                                        streamID,
                                                        flags);
}

ElasticFrameMessages EFPSignalSend::registerContent(EFPStreamContent &content) {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  uint8_t streamID = content.mGStreamID;
  ElasticFrameContent dataContent = content.mGFrameContent;

  if (isKnown[streamID][dataContent]) {
    return ElasticFrameMessages::contentAlreadyListed;
  }
  efpStreamLists[streamID].push_back(content);
  isKnown[streamID][dataContent] = true;
  LOGGER(true, LOGG_NOTIFY, "Added entry")
  mStreamListChanged = true;
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::deleteContent(ElasticFrameContent dataContent, uint8_t streamID){
  std::lock_guard<std::mutex> lock(mStreamListMtx);

  if (!isKnown[streamID][dataContent]) {
    return ElasticFrameMessages::contentNotListed;
  }
  int lItemIndex = 0;
  std::vector<EFPStreamContent> *streamContent = &efpStreamLists[streamID];
  for (auto &rItems: *streamContent) {
    if (rItems.mGFrameContent == dataContent) {
      streamContent->erase(streamContent->begin() + lItemIndex);
      LOGGER(true, LOGG_NOTIFY, "Deleted entry")
      isKnown[streamID][dataContent] = false;
      mStreamListChanged = true;
    } else {
      lItemIndex++;
    }
  }

  if (isKnown[streamID][dataContent]) {
    return ElasticFrameMessages::deleteContentFail;
  }
  return ElasticFrameMessages::noError;
}



void EFPSignalSend::signalWorker() {
  mSignalThreadActive = true;
  while (mThreadRunSignal) {
    //Run signal worker 10 times per second
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    mStreamListMtx.lock();
    for (int x = 0; x < UINT8_MAX; x++) {
      std::vector<EFPStreamContent> *streamContent = &efpStreamLists[x];
      if (streamContent->size()) {
        int lItemIndex = 0;
        for (auto &rItems: *streamContent) {
          if (!rItems.isStillAlive(100)) {
            streamContent->erase(streamContent->begin() + lItemIndex);
            LOGGER(true, LOGG_NOTIFY, "Deleted entry")
            mStreamListChanged = true;
          } else {
            lItemIndex++;
          }
        }
      }
    }
    mStreamListMtx.unlock();

    if (mStreamListChanged) {
      LOGGER(true, LOGG_NOTIFY, "ListChanged")
      mStreamListChanged = false;
    }
    //Should embedd
  }
  mSignalThreadActive = false;
  LOGGER(true, LOGG_NOTIFY, "signalWorker exit")
}

void EFPSignalSend::startSignalWorker() {
  std::thread(std::bind(&EFPSignalSend::signalWorker, this)).detach();
}