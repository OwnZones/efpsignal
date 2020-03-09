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
    if (mAutoRegister) {
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
      mEFPStreamListVersion++;
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

  ElasticFrameMessages signalMsg= signalFilter(dataContent, streamID);
  if (signalMsg != ElasticFrameMessages::noError) {
    return signalMsg;
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
  ElasticFrameMessages signalMsg = signalFilter(dataContent, streamID);
  if (signalMsg != ElasticFrameMessages::noError) {
    return signalMsg;
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
  uint8_t streamID = content.mGStreamID;
  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }

  ElasticFrameContent dataContent = content.mGFrameContent;
  if (isKnown[streamID][dataContent]) {
    return ElasticFrameMessages::contentAlreadyListed;
  }

  std::lock_guard<std::mutex> lock(mStreamListMtx);
  efpStreamLists[streamID].push_back(content);
  isKnown[streamID][dataContent] = true;
  mEFPStreamListVersion++;
  LOGGER(true, LOGG_NOTIFY, "Added entry")
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::deleteContent(ElasticFrameContent dataContent, uint8_t streamID){

  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }

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
      mEFPStreamListVersion++;
    } else {
      lItemIndex++;
    }
  }
  if (isKnown[streamID][dataContent]) {
    return ElasticFrameMessages::deleteContentFail;
  }
  return ElasticFrameMessages::noError;
}


json EFPSignalSend::generateStreamInfo(EFPStreamContent &content) {
  json jsonStream;

  //General part
  jsonStream["gdescription_str"] = content.mGDescription;
  jsonStream["gframecontent_u8"] = content.mGFrameContent;
  jsonStream["gstreamid_u8"] = content.mGStreamID;
  jsonStream["gprotectiongroup_u8"] = content.mGProtectionGroupID = 0;
  jsonStream["gsyncgroup_u8"] = content.mGSyncGroupID = 0;
  jsonStream["gpriority_u8"] = content.mGPriority = 0;
  jsonStream["gnotifyhere_u64"] = content.mGNotifyHere = 0;

  //Video part
  jsonStream["vratenum_u32"] = content.mVFrameRateNum = 0;
  jsonStream["vrateden_u32"] = content.mVFrameRateDen = 0;
  jsonStream["vwidth_u32"] = content.mVWidth = 0;
  jsonStream["vheight_u32"] = content.mVHeight = 0;
  jsonStream["vbps_u32"] = content.mVBitsPerSec = 0;

  //Audio part
  jsonStream["afreqnum_u32"] = content.mAFreqNum = 0;
  jsonStream["afreqden_u32"] = content.mAFreqDen = 0;
  jsonStream["anoch_u32"] = content.mANoChannels = 0;
  jsonStream["achmap_u32"] = content.mAChannelMapping = 0;
  jsonStream["abps_u32"] = content.mABitsPerSec = 0;

  //Text part
  jsonStream["ttype_u32"] = content.mTTextType = 0;
  jsonStream["tlang_str"] = content.mTLanguage = "";

  //auX part
  jsonStream["xtype_u32"] = content.mXType = 0;
  jsonStream["xstr_str"] = content.mXString = "";
  jsonStream["xval_u32"] = content.mXValue = 0;
  return jsonStream;
}

json EFPSignalSend::generateAllStreamInfo() {
  json jsonAllStreams, tempStreams;
  for (int x = 0; x < UINT8_MAX; x++) {
    std::vector<EFPStreamContent> *streamContent = &efpStreamLists[x];
    if (streamContent->size()) {
      for (auto &rItems: *streamContent) {
        json jsonStream = generateStreamInfo(rItems);
        tempStreams.push_back(jsonStream);
      }
    }
  }
  jsonAllStreams["efpstreamversion"] = mEFPStreamListVersion;
  jsonAllStreams["efpsignalversion"] = EFP_SIGNAL_VERSION;
  jsonAllStreams["efpstreams"] = tempStreams;
  return jsonAllStreams;
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
            mEFPStreamListVersion++;
          } else {
            lItemIndex++;
          }
        }
      }
    }


    if (mEFPStreamListVersion != mOldEFPStreamListVersion) {
      LOGGER(true, LOGG_NOTIFY, "ListChanged")
      mOldEFPStreamListVersion = mEFPStreamListVersion;
    }

    mStreamListMtx.unlock();
    //Should embedd
  }
  mSignalThreadActive = false;
  LOGGER(true, LOGG_NOTIFY, "signalWorker exit")
}

void EFPSignalSend::startSignalWorker() {
  std::thread(std::bind(&EFPSignalSend::signalWorker, this)).detach();
}