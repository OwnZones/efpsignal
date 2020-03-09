//
// Created by Anders Cedronius on 2020-03-06.
//

#include "efpsignal.h"
#include "efpsignalinternal.h"


//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Sender
//
//
//---------------------------------------------------------------------------------------------------------------------

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
  if (!mIsKnown[streamID][dataContent]) {
    if (mAutoRegister) {
      EFPStreamContent newContent(mGarbageCollectms);
      newContent.mGStreamID = streamID;
      newContent.mGFrameContent = dataContent;
      if (declareContentCallback) {
        declareContentCallback(&newContent);
      }
      std::lock_guard<std::mutex> lock(mStreamListMtx);
      mEFPStreamLists[streamID].push_back(newContent);
      mIsKnown[streamID][dataContent] = true;
      LOGGER(true, LOGG_NOTIFY, "Added entry")
      mEFPStreamListVersion++;
    }
    if (mDropUnknown) {
      return ElasticFrameMessages::efpSignalDropped;
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

  ElasticFrameMessages signalMsg = signalFilter(dataContent, streamID);
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
  if (mIsKnown[streamID][dataContent]) {
    return ElasticFrameMessages::contentAlreadyListed;
  }

  std::lock_guard<std::mutex> lock(mStreamListMtx);
  mEFPStreamLists[streamID].push_back(content);
  mIsKnown[streamID][dataContent] = true;
  mEFPStreamListVersion++;
  LOGGER(true, LOGG_NOTIFY, "Added entry")
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::deleteContent(ElasticFrameContent dataContent, uint8_t streamID) {
  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }

  std::lock_guard<std::mutex> lock(mStreamListMtx);

  if (!mIsKnown[streamID][dataContent]) {
    return ElasticFrameMessages::contentNotListed;
  }

  int lItemIndex = 0;
  std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[streamID];
  for (auto &rItems: *streamContent) {
    if (rItems.mGFrameContent == dataContent) {
      streamContent->erase(streamContent->begin() + lItemIndex);
      LOGGER(true, LOGG_NOTIFY, "Deleted entry")
      mIsKnown[streamID][dataContent] = false;
      mEFPStreamListVersion++;
    } else {
      lItemIndex++;
    }
  }
  if (mIsKnown[streamID][dataContent]) {
    return ElasticFrameMessages::deleteContentFail;
  }
  return ElasticFrameMessages::noError;
}

EFPStreamContent EFPSignalSend::getContent(ElasticFrameContent dataContent, uint8_t streamID) {
  if (streamID == 0) {
    return 0;
  }
  if (!mIsKnown[streamID][dataContent]) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[streamID];
  for (auto &rItems: *streamContent) {
    if (rItems.mGFrameContent == dataContent) {
      return rItems;
    }
  }
  return 0;
}

json EFPSignalSend::generateStreamInfoFromBinary(EFPStreamContent &content) {
  json jsonStream;

  //General part
  jsonStream["gdescription_str"] = content.mGDescription;
  jsonStream["gframecontent_u8"] = content.mGFrameContent;
  jsonStream["gstreamid_u8"] = content.mGStreamID;
  jsonStream["gprotectiongroup_u8"] = content.mGProtectionGroupID;
  jsonStream["gsyncgroup_u8"] = content.mGSyncGroupID;
  jsonStream["gpriority_u8"] = content.mGPriority;
  jsonStream["gnotifyhere_u64"] = content.mGNotifyHere;

  //Video part
  jsonStream["vratenum_u32"] = content.mVFrameRateNum;
  jsonStream["vrateden_u32"] = content.mVFrameRateDen;
  jsonStream["vwidth_u32"] = content.mVWidth;
  jsonStream["vheight_u32"] = content.mVHeight;
  jsonStream["vbps_u32"] = content.mVBitsPerSec;

  //Audio part
  jsonStream["afreqnum_u32"] = content.mAFreqNum;
  jsonStream["afreqden_u32"] = content.mAFreqDen;
  jsonStream["anoch_u32"] = content.mANoChannels;
  jsonStream["achmap_u32"] = content.mAChannelMapping;
  jsonStream["abps_u32"] = content.mABitsPerSec;

  //Text part
  jsonStream["ttype_u32"] = content.mTTextType;
  jsonStream["tlang_str"] = content.mTLanguage;

  //auX part
  jsonStream["xtype_u32"] = content.mXType;
  jsonStream["xstr_str"] = content.mXString;
  jsonStream["xval_u32"] = content.mXValue;
  return jsonStream;
}

uint32_t EFPSignalSend::signalVersion() {
  return EFP_SIGNAL_VERSION;
}

json EFPSignalSend::generateAllStreamInfo() {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  json jsonAllStreams, tempStreams;
  for (int x = 0; x < UINT8_MAX; x++) {
    std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[x];
    if (streamContent->size()) {
      for (auto &rItems: *streamContent) {
        json jsonStream = generateStreamInfoFromBinary(rItems);
        tempStreams.push_back(jsonStream);
      }
    }
  }
  jsonAllStreams["efpstreamversion_u32"] = mEFPStreamListVersion;
  jsonAllStreams["efpsignalversion_u32"] = EFP_SIGNAL_VERSION;
  jsonAllStreams["efpstreams_arr"] = tempStreams;
  return jsonAllStreams;
}

void EFPSignalSend::signalWorker() {
  mSignalThreadActive = true;
  while (mThreadRunSignal) {
    //Run signal worker 10 times per second
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mStreamListMtx.lock();
    for (int x = 0; x < UINT8_MAX; x++) {
      std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[x];
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
    mStreamListMtx.unlock();

    if (mEFPStreamListVersion != mOldEFPStreamListVersion) {
      LOGGER(true, LOGG_NOTIFY, "ListChanged")
      mOldEFPStreamListVersion = mEFPStreamListVersion;
      mChangeDetected = true;
    }

    if (!mEmbedInterval100msStepsFireCounter) {
      mEmbedInterval100msStepsFireCounter = mEmbedInterval100msSteps;
      if (mEmbeddInStream) {
        json embeddedData;
        if (mEmbeddOnlyChanges && mChangeDetected) {
          mChangeDetected = false;
          embeddedData = generateAllStreamInfo();
          std::string jsonString = embeddedData.dump();
          std::vector<uint8_t> jsondata(jsonString.begin(), jsonString.end());
          ElasticFrameProtocolSender::packAndSend(jsondata,
                                                  ElasticFrameContent::efpsig,
                                                  0,
                                                  0,
                                                  EFP_CODE('J', 'S', 'O', 'N'),
                                                  0,
                                                  0);
        } else {
          embeddedData = generateAllStreamInfo();
          std::string jsonString = embeddedData.dump();
          std::vector<uint8_t> jsondata(jsonString.begin(), jsonString.end());
          ElasticFrameProtocolSender::packAndSend(jsondata,
                                                  ElasticFrameContent::efpsig,
                                                  0,
                                                  0,
                                                  EFP_CODE('J', 'S', 'O', 'N'),
                                                  0,
                                                  0);
        }
      }
    }

    mEmbedInterval100msStepsFireCounter--;
  }
  mSignalThreadActive = false;
  LOGGER(true, LOGG_NOTIFY, "signalWorker exit")
}

void EFPSignalSend::startSignalWorker() {
  std::thread(std::bind(&EFPSignalSend::signalWorker, this)).detach();
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Receiver
//
//
//---------------------------------------------------------------------------------------------------------------------

EFPSignalReceive::EFPSignalReceive() {
  ElasticFrameProtocolReceiver::receiveCallback = std::bind(&EFPSignalReceive::gotData, this, std::placeholders::_1);
  LOGGER(true, LOGG_NOTIFY, "EFPSignalReceive construct")
};

EFPSignalReceive::~EFPSignalReceive() {
  LOGGER(true, LOGG_NOTIFY, "EFPSignalReceive destruct")
}

uint32_t EFPSignalReceive::signalVersion() {
  return EFP_SIGNAL_VERSION;
}

ElasticFrameMessages EFPSignalReceive::getStreamInformation(uint8_t *data, size_t size, EFPSignalReceive::EFPSignalReceiveData* parsedData) {
  bool jsonOK = true;
  std::string content((const char *) data, (const char *) data + size);
  json j, jError;

  try {
    j = json::parse(content.c_str());
  } catch (const std::exception &e) {
    LOGGER(true, LOGG_ERROR, "Error reading json data -> " << e.what())
    return ElasticFrameMessages::contentNotListed; //Fixme
  }

  parsedData->signalVersion = getContentForKey<uint32_t>("efpsignalversion_u32", j, jError, jsonOK);
  if (!jsonOK)
    return ElasticFrameMessages::contentNotListed; //Fixme
  parsedData->streamVersion = getContentForKey<uint32_t>("efpstreamversion_u32", j, jError, jsonOK);
  if (!jsonOK)
    return ElasticFrameMessages::contentNotListed; //Fixme
  //auto streams = getContentForKey<std::string>("efpstreams_arr", j, jError, jsonOK);

  try {
    auto streams = j["efpstreams_arr"];

    for (auto &element : streams) {
      EFPStreamContent newContent(UINT32_MAX);
      //General part
      newContent.mGDescription = getContentForKey<std::string>("gdescription_str", element, jError, jsonOK);
      newContent.mGFrameContent = getContentForKey<ElasticFrameContent>("gframecontent_u8", element, jError, jsonOK);
      newContent.mGStreamID = getContentForKey<uint8_t>("gstreamid_u8", element, jError, jsonOK);
      newContent.mGProtectionGroupID = getContentForKey<uint8_t>("gprotectiongroup_u8", element, jError, jsonOK);
      newContent.mGSyncGroupID = getContentForKey<uint8_t>("gsyncgroup_u8", element, jError, jsonOK);
      newContent.mGPriority = getContentForKey<uint8_t>("gpriority_u8", element, jError, jsonOK);
      newContent.mGNotifyHere = getContentForKey<uint64_t>("gnotifyhere_u64", element, jError, jsonOK);

      //Video part
      newContent.mVFrameRateNum = getContentForKey<uint32_t>("vratenum_u32", element, jError, jsonOK);
      newContent.mVFrameRateDen = getContentForKey<uint32_t>("vrateden_u32", element, jError, jsonOK);
      newContent.mVWidth = getContentForKey<uint32_t>("vwidth_u32", element, jError, jsonOK);
      newContent.mVHeight = getContentForKey<uint32_t>("vheight_u32", element, jError, jsonOK);
      newContent.mVBitsPerSec = getContentForKey<uint32_t>("vbps_u32", element, jError, jsonOK);

      //Audio part
      newContent.mAFreqNum = getContentForKey<uint32_t>("afreqnum_u32", element, jError, jsonOK);
      newContent.mAFreqDen = getContentForKey<uint32_t>("afreqden_u32", element, jError, jsonOK);
      newContent.mANoChannels = getContentForKey<uint32_t>("anoch_u32", element, jError, jsonOK);
      newContent.mAChannelMapping = getContentForKey<uint32_t>("achmap_u32", element, jError, jsonOK);

      //Text part
      newContent.mTTextType = getContentForKey<uint32_t>("ttype_u32", element, jError, jsonOK);
      newContent.mTLanguage = getContentForKey<std::string>("tlang_str", element, jError, jsonOK);

      //auX part
      newContent.mXType = getContentForKey<uint32_t>("xtype_u32", element, jError, jsonOK);
      newContent.mXString = getContentForKey<std::string>("xstr_str", element, jError, jsonOK);
      newContent.mXValue = getContentForKey<uint32_t>("xval_u32", element, jError, jsonOK);

      if (jsonOK) {
        parsedData->contentList.push_back(newContent);
      } else {
        LOGGER(true, LOGG_ERROR, "ERROR parsing EFPStreamContent")
        jsonOK = true;
      }
    }
  } catch (const std::exception &e) {
    LOGGER(true, LOGG_ERROR, "Error reading json data -> " << e.what())
    return ElasticFrameMessages::contentNotListed;
  }
  return ElasticFrameMessages::noError;
}

void EFPSignalReceive::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
  if (packet->mDataContent == ElasticFrameContent::efpsig) {
//    std::cout << "got signaling data" << std::endl;

    if (this->contentInformationCallback) {
      EFPSignalReceiveData dataFromEFPSignal;
      ElasticFrameMessages status = getStreamInformation(packet->pFrameData, packet->mFrameSize, &dataFromEFPSignal);
      if (status != ElasticFrameMessages::noError) {
        LOGGER(true, LOGG_ERROR, "ERROR parsing EFPStreamContent")
        return;
      }

      this->contentInformationCallback(dataFromEFPSignal);
      return;

    } else {
      LOGGER(true, LOGG_ERROR, "contentInformationCallback not defined")
      return;

  }

    if (this->receiveCallback) {
      this->receiveCallback(packet);
    }
  }
}
