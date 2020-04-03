//
// Created by Anders Cedronius on 2020-03-06.
//

#include "efpsignal.h"
#include "efpsignalinternal.h"

//Global
static void EFPSignalExtraktValuesForKeyV1(EFPStreamContent &newContent, json &element, json &jError, bool &jsonOK) {
    //General part
    newContent.mVariables.mGFrameContent =
            EFPGetContentForKey<ElasticFrameContent>("gframecontent_u8", element, jError, jsonOK);
    newContent.mVariables.mGStreamID = EFPGetContentForKey<uint8_t>("gstreamid_u8", element, jError, jsonOK);
    newContent.mVariables.mGChanged = EFPGetContentForKey<uint8_t>("gchanged_u8", element, jError, jsonOK);
    newContent.mVariables.mGProtectionGroupID =
            EFPGetContentForKey<uint8_t>("gprotectiongroup_u8", element, jError, jsonOK);
    newContent.mVariables.mGSyncGroupID = EFPGetContentForKey<uint8_t>("gsyncgroup_u8", element, jError, jsonOK);
    newContent.mVariables.mGPriority = EFPGetContentForKey<uint8_t>("gpriority_u8", element, jError, jsonOK);
    newContent.mVariables.mGNotifyHere = EFPGetContentForKey<uint64_t>("gnotifyhere_u64", element, jError, jsonOK);
    newContent.mVariables.mGPTSDTSBase = EFPGetContentForKey<uint32_t>("mgptsdtsbase_u32", element, jError, jsonOK);

    //Video part
    newContent.mVariables.mVFrameRateNum = EFPGetContentForKey<uint32_t>("vratenum_u32", element, jError, jsonOK);
    newContent.mVariables.mVFrameRateDen = EFPGetContentForKey<uint32_t>("vrateden_u32", element, jError, jsonOK);
    newContent.mVariables.mVWidth = EFPGetContentForKey<uint32_t>("vwidth_u32", element, jError, jsonOK);
    newContent.mVariables.mVHeight = EFPGetContentForKey<uint32_t>("vheight_u32", element, jError, jsonOK);
    newContent.mVariables.mVBitsPerSec = EFPGetContentForKey<uint32_t>("vbps_u32", element, jError, jsonOK);

    //Audio part
    newContent.mVariables.mAFreq = EFPGetContentForKey<uint32_t>("afreq_u32", element, jError, jsonOK);
    newContent.mVariables.mANoChannels = EFPGetContentForKey<uint32_t>("anoch_u32", element, jError, jsonOK);
    newContent.mVariables.mAChannelMapping = EFPGetContentForKey<uint32_t>("achmap_u32", element, jError, jsonOK);

    //Text part
    std::string tmpStr = EFPGetContentForKey<std::string>("tlang_str", element, jError, jsonOK);
    if ((tmpStr.size() + 1) > sizeof(newContent.mVariables.mTLanguage)) {
        jError.push_back("tlang_str too large");
        jsonOK = false;
    } else {
        strncpy(&newContent.mVariables.mTLanguage[0], tmpStr.c_str(), tmpStr.size() + 1);
    }

    tmpStr = EFPGetContentForKey<std::string>("ttextconfig_str", element, jError, jsonOK);
    if ((tmpStr.size() + 1) > sizeof(newContent.mVariables.mTextConfig)) {
        jError.push_back("ttextconfig_str too large");
        jsonOK = false;
    } else {
        strncpy(&newContent.mVariables.mTextConfig[0], tmpStr.c_str(), tmpStr.size() + 1);
    }

    //auX part
    newContent.mVariables.mXType = EFPGetContentForKey<uint32_t>("xtype_u32", element, jError, jsonOK);

    tmpStr = EFPGetContentForKey<std::string>("xstr_str", element, jError, jsonOK);
    if ((tmpStr.size() + 1) > sizeof(newContent.mVariables.mXString)) {
        jError.push_back("xstr_str too large");
        jsonOK = false;
    } else {
        strncpy(&newContent.mVariables.mXString[0], tmpStr.c_str(), tmpStr.size() + 1);
    }

    newContent.mVariables.mXValue = EFPGetContentForKey<uint32_t>("xval_u32", element, jError, jsonOK);
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Sender
//
//
//---------------------------------------------------------------------------------------------------------------------

EFPSignalSend::EFPSignalSend(uint16_t setMTU, uint32_t garbageCollectms) : ElasticFrameProtocolSender(setMTU){
  mEFPStreamLists.reserve(256);
  mGarbageCollectms = garbageCollectms;
  startSignalWorker();
  LOGGER(true, LOGG_NOTIFY, "EFPSignalSend construct")
}

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
  LOGGER(true, LOGG_NOTIFY, "EFPSignalSend destruct")
}

ElasticFrameMessages EFPSignalSend::signalFilter(ElasticFrameContent dataContent, uint8_t streamID, uint32_t *dataMessage) {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  if (mIsKnown[streamID][dataContent]) {
    std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[streamID];
    for (auto &rItem: *lStreamContent) {
      if (rItem.mVariables.mGFrameContent == dataContent) {
        rItem.resetTTL();

        if (rItem.mVariables.mGSyncGroupID) {
          *dataMessage = 1; //Signal the frame is part of a sync group
        }

        if (!rItem.mWhiteListed) {
          return ElasticFrameMessages::efpSignalDropped;
        }
        break;
      }
    }
  } else {
    if (mAutoRegister) {
      EFPStreamContent lNewContent(mGarbageCollectms);
      lNewContent.mVariables.mGStreamID = streamID;
      lNewContent.mVariables.mGFrameContent = dataContent;
      if (declareContentCallback) {
        lNewContent.mWhiteListed = declareContentCallback(lNewContent);
      }
      lNewContent.mVariables.mGChanged = 1;
      mEFPStreamLists[streamID].push_back(lNewContent);
      mIsKnown[streamID][dataContent] = true;
      LOGGER(true, LOGG_NOTIFY, "Added entry")
      mEFPStreamListVersion++;
      if (!lNewContent.mWhiteListed) {
        return ElasticFrameMessages::efpSignalDropped;
      }
    }
  }
  return ElasticFrameMessages::noError;
}



ElasticFrameMessages
EFPSignalSend::packAndSend(const std::vector<uint8_t> &rPacket,
                           ElasticFrameContent frameContent,
                           uint64_t pts,
                           uint64_t dts,
                           uint32_t code,
                           uint8_t streamID,
                           uint8_t flags,
                           std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t streamID)> sendFunction) {

  if (frameContent != ElasticFrameContent::efpsig) {
    uint32_t lSignalFilterDataMessage = 0;
    ElasticFrameMessages lSignalMsg = signalFilter(frameContent, streamID, &lSignalFilterDataMessage);
    if (lSignalFilterDataMessage == 1) {

    }
    if (lSignalMsg != ElasticFrameMessages::noError) {
      return lSignalMsg;
    }
  }
  return ElasticFrameProtocolSender::packAndSend(rPacket, frameContent, pts, dts, code, streamID, flags, sendFunction);
}

ElasticFrameMessages
EFPSignalSend::packAndSendFromPtr(const uint8_t *pPacket,
                                  size_t packetSize,
                                  ElasticFrameContent frameContent,
                                  uint64_t pts,
                                  uint64_t dts,
                                  uint32_t code,
                                  uint8_t streamID,
                                  uint8_t flags,
                                  std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t streamID)> sendFunction) {
  if (frameContent != ElasticFrameContent::efpsig) {
    uint32_t lSignalFilterDataMessage = 0;
    ElasticFrameMessages lSignalMsg = signalFilter(frameContent, streamID, &lSignalFilterDataMessage);
    if (lSignalFilterDataMessage == 1) {

    }

    if (lSignalMsg != ElasticFrameMessages::noError) {
      return lSignalMsg;
    }
  }
  return ElasticFrameProtocolSender::packAndSendFromPtr(pPacket,
                                                        packetSize,
                                                        frameContent,
                                                        pts,
                                                        dts,
                                                        code,
                                                        streamID,
                                                        flags,
                                                        sendFunction);
}

ElasticFrameMessages EFPSignalSend::clearAllContent() {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  for (int x = 0; x < UINT8_MAX; x++) {
    for (int y = 0; y < UINT8_MAX; y++) {
      mIsKnown[x][y] = false;
    }
    std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[x];
    if (lStreamContent->size()) {
      lStreamContent->clear();
    }
  }
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::addContent(EFPStreamContent &rStreamContent) {
  uint8_t lStreamID = rStreamContent.mVariables.mGStreamID;
  if (lStreamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }

  ElasticFrameContent lDataContent = rStreamContent.mVariables.mGFrameContent;
  if (mIsKnown[lStreamID][lDataContent]) {
    return ElasticFrameMessages::contentAlreadyListed;
  }

  std::lock_guard<std::mutex> lock(mStreamListMtx);
  rStreamContent.mVariables.mGChanged = 1;
  mEFPStreamLists[lStreamID].push_back(rStreamContent);
  mIsKnown[lStreamID][lDataContent] = true;
  mEFPStreamListVersion++;
  LOGGER(true, LOGG_NOTIFY, "Added entry")
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::deleteContent(ElasticFrameContent frameContent, uint8_t streamID) {
  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  if (!mIsKnown[streamID][frameContent]) {
    return ElasticFrameMessages::contentNotListed;
  }

  int lItemIndex = 0;
  std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[streamID];
  std::vector<EFPStreamContent> lDeletedItems;
  for (auto &rItem: *lStreamContent) {
    if (rItem.mVariables.mGFrameContent == frameContent) {
      lDeletedItems.push_back(rItem);
      rItem.mVariables.mGChanged = 3;
      lStreamContent->erase(lStreamContent->begin() + lItemIndex);
      LOGGER(true, LOGG_NOTIFY, "Deleted entry")
      mIsKnown[streamID][frameContent] = false;
      mEFPStreamListVersion++;
    } else {
      lItemIndex++;
    }
  }
  if (mIsKnown[streamID][frameContent]) {
    return ElasticFrameMessages::deleteContentFail;
  }

  //Fixme. deal with deleted items.

  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::modifyContent(ElasticFrameContent frameContent, uint8_t streamID, std::function<void(EFPStreamContent &)> function){
  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  if (!mIsKnown[streamID][frameContent]) {
    return ElasticFrameMessages::contentNotListed;
  }

  bool lFound = false;
  std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[streamID];
  for (auto &rItem: *lStreamContent) {
    if (rItem.mVariables.mGFrameContent == frameContent) {
      function(rItem);
      rItem.mVariables.mGChanged= 2;
      lFound = true;
      break;
    }
  }
  if (!lFound) {
    return ElasticFrameMessages::contentNotListed;
  }
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::getContent(EFPStreamContent &rStreamContent,
                                               ElasticFrameContent frameContent,
                                               uint8_t streamID) {
  if (streamID == 0) {
    return ElasticFrameMessages::reservedStreamValue;
  }
  if (!mIsKnown[streamID][frameContent]) {
    return ElasticFrameMessages::contentNotListed;
  }
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[streamID];
  for (auto &rItem: *lStreamContent) {
    if (rItem.mVariables.mGFrameContent == frameContent) {
      rStreamContent = rItem; //This should implicitly create a copy. since rStreamContent and rItem are references.
      // Think about this -> rItem.mVariables.mGChanged = 0; //We peeked at the content it's not marked as changed anymore.
      break;
    }
  }
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::generateJSONStreamInfoFromData(json &rJsonContent, EFPStreamContent &rStreamContent) {

  //General part
  rJsonContent["gframecontent_u8"] = rStreamContent.mVariables.mGFrameContent;
  rJsonContent["gstreamid_u8"] = rStreamContent.mVariables.mGStreamID;
  rJsonContent["gchanged_u8"] = rStreamContent.mVariables.mGChanged;
  rJsonContent["gprotectiongroup_u8"] = rStreamContent.mVariables.mGProtectionGroupID;
  rJsonContent["gsyncgroup_u8"] = rStreamContent.mVariables.mGSyncGroupID;
  rJsonContent["gpriority_u8"] = rStreamContent.mVariables.mGPriority;
  rJsonContent["gnotifyhere_u64"] = rStreamContent.mVariables.mGNotifyHere;
  rJsonContent["mgptsdtsbase_u32"] = rStreamContent.mVariables.mGPTSDTSBase;

  //Video part
  rJsonContent["vratenum_u32"] = rStreamContent.mVariables.mVFrameRateNum;
  rJsonContent["vrateden_u32"] = rStreamContent.mVariables.mVFrameRateDen;
  rJsonContent["vwidth_u32"] = rStreamContent.mVariables.mVWidth;
  rJsonContent["vheight_u32"] = rStreamContent.mVariables.mVHeight;
  rJsonContent["vbps_u32"] = rStreamContent.mVariables.mVBitsPerSec;

  //Audio part
  rJsonContent["afreq_u32"] = rStreamContent.mVariables.mAFreq;
  rJsonContent["anoch_u32"] = rStreamContent.mVariables.mANoChannels;
  rJsonContent["achmap_u32"] = rStreamContent.mVariables.mAChannelMapping;
  rJsonContent["abps_u32"] = rStreamContent.mVariables.mABitsPerSec;

  //Text part
  rJsonContent["tlang_str"] = rStreamContent.mVariables.mTLanguage;
  rJsonContent["ttextconfig_str"] = rStreamContent.mVariables.mTextConfig;

  //auX part
  rJsonContent["xtype_u32"] = rStreamContent.mVariables.mXType;
  rJsonContent["xstr_str"] = rStreamContent.mVariables.mXString;
  rJsonContent["xval_u32"] = rStreamContent.mVariables.mXValue;
  return ElasticFrameMessages::noError;
}

uint32_t EFPSignalSend::signalVersion() {
  return EFP_SIGNAL_VERSION;
}

ElasticFrameMessages EFPSignalSend::generateAllStreamInfoJSON(json &rJsonContent,  bool deltasOnly) {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  json lTempStreams;
  for (int x = 0; x < UINT8_MAX; x++) {
    std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[x];
    if (lStreamContent->size()) {
      for (auto &rItem: *lStreamContent) {
        if ((deltasOnly && rItem.mVariables.mGChanged) || !deltasOnly) {
          json jsonStream;
          ElasticFrameMessages status = generateJSONStreamInfoFromData(jsonStream, rItem);
          rItem.mVariables.mGChanged = 0;
          if (status == ElasticFrameMessages::noError) {
            lTempStreams.push_back(jsonStream);
          } else {
            LOGGER(true, LOGG_ERROR, "ERROR generateAllStreamInfoJSON")
          }
        }
      }
    }
  }
  rJsonContent["efpstreamversion_u32"] = mEFPStreamListVersion;
  rJsonContent["efpsignalversion_u32"] = EFP_SIGNAL_VERSION;
  rJsonContent["efpstreams_arr"] = lTempStreams;
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::generateDataStreamInfoFromJSON(EFPStreamContent &rStreamContent, json &rJsonContent) {
  bool lJsonOK = true;
  json jError;
  EFPSignalExtraktValuesForKeyV1(rStreamContent, rJsonContent, jError, lJsonOK);
  if (lJsonOK) {
    return ElasticFrameMessages::noError;
  }
  LOGGER(true, LOGG_ERROR, "ERROR parsing JSON EFPStreamContent")
  return ElasticFrameMessages::dataNotJSON;
}

ElasticFrameMessages EFPSignalSend::generateAllStreamInfoData(std::unique_ptr<std::vector<uint8_t>> &rStreamContentData,  bool deltasOnly) {
  std::lock_guard<std::mutex> lock(mStreamListMtx);
  uint16_t lTotalBlocks = 0;
  for (int x = 0; x < UINT8_MAX; x++) {
    std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[x];
    if (streamContent->size()) {
      for (auto &rItem: *streamContent) {
        if ((deltasOnly && rItem.mVariables.mGChanged) || !deltasOnly) {
          lTotalBlocks++;
        }
      }
    }
   // lTotalBlocks += mEFPStreamLists[x].size();
  }
  size_t lTotalByteSize = lTotalBlocks * sizeof(EFPStreamContent::Variables);
  lTotalByteSize += sizeof(EFPStreamContent::BinaryHeaderV1);
  std::unique_ptr<std::vector<uint8_t>> lAllData = std::make_unique<std::vector<uint8_t>>(lTotalByteSize);
  uint8_t *lAllDataPtr = lAllData->data();
  EFPStreamContent::BinaryHeaderV1 lHeader;
  lHeader.mEFPStreamVersion = mEFPStreamListVersion;
  lHeader.mNumBlocks = lTotalBlocks;
  std::memmove(lAllDataPtr, &lHeader, sizeof(EFPStreamContent::BinaryHeaderV1));
  lAllDataPtr += sizeof(EFPStreamContent::BinaryHeaderV1);

  for (int x = 0; x < UINT8_MAX; x++) {
    std::vector<EFPStreamContent> *streamContent = &mEFPStreamLists[x];
    if (streamContent->size()) {
      for (auto &rItem: *streamContent) {
        if ((deltasOnly && rItem.mVariables.mGChanged) || !deltasOnly) {
          std::memmove(lAllDataPtr, &rItem.mVariables, sizeof(EFPStreamContent::Variables));
          rItem.mVariables.mGChanged = 0;
          lAllDataPtr += sizeof(EFPStreamContent::Variables);
        }
      }
    }
  }
  rStreamContentData = std::move(lAllData);
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalSend::sendEmbeddedData(bool deltasOnly) {
  if (mBinaryMode) {
    std::unique_ptr<std::vector<uint8_t>> lRawData;
    ElasticFrameMessages lStatus = generateAllStreamInfoData(lRawData, deltasOnly);
    if (lStatus == ElasticFrameMessages::noError) {
      if (mEmbedInStream) {
        ElasticFrameProtocolSender::packAndSend(*lRawData,
                                                ElasticFrameContent::efpsig,
                                                0,
                                                0,
                                                EFP_CODE('D', 'A', 'T', 'A'),
                                                0,
                                                0);
      } else {
        if (declarationCallback) {
          json lNothing;
          declarationCallback(lRawData, lNothing);
        }
      }
    }
  } else {
    json lEmbeddedData;
    ElasticFrameMessages lStatus = generateAllStreamInfoJSON(lEmbeddedData, deltasOnly);
    if (lStatus == ElasticFrameMessages::noError) {
      std::string jsonString = lEmbeddedData.dump();
      std::vector<uint8_t> jsondata(jsonString.begin(), jsonString.end());
      if (mEmbedInStream) {
        ElasticFrameProtocolSender::packAndSend(jsondata,
                                                ElasticFrameContent::efpsig,
                                                0,
                                                0,
                                                EFP_CODE('J', 'S', 'O', 'N'),
                                                0,
                                                0);
      } else {
        if (declarationCallback) {
          std::unique_ptr<std::vector<uint8_t>> lNothing = nullptr;
          declarationCallback(lNothing, lEmbeddedData);
        }
      }
    }
  }
  return ElasticFrameMessages::noError;
}

void EFPSignalSend::signalWorker() {
  mSignalThreadActive = true;
  while (mThreadRunSignal) {
    //Run signal worker 10 times per second
    std::this_thread::sleep_for(std::chrono::milliseconds(100));


    { //  < Lock wrapper start >
      std::lock_guard<std::mutex> lock(mStreamListMtx);
      for (int x = 0; x < UINT8_MAX; x++) {
        std::vector<EFPStreamContent> *lStreamContent = &mEFPStreamLists[x];
        if (lStreamContent->size()) {
          int lItemIndex = 0;
          for (auto &rItem: *lStreamContent) {
            if (!rItem.isStillAlive(100)) {
              lStreamContent->erase(lStreamContent->begin() + lItemIndex);
              LOGGER(true, LOGG_NOTIFY, "Deleted entry")
              mEFPStreamListVersion++;
            } else {
              lItemIndex++;
            }
          }
        }
      }
    } //  < Lock wrapper end >

    if (mEFPStreamListVersion != mOldEFPStreamListVersion) {
      LOGGER(true, LOGG_NOTIFY, "ListChanged")
      mOldEFPStreamListVersion = mEFPStreamListVersion;
      mChangeDetected = true;
    }

    if (!mEmbedInterval100msStepsFireCounter) {
      mEmbedInterval100msStepsFireCounter = mEmbedInterval100msSteps;
      if (mTriggerChanges && mChangeDetected) {
        mChangeDetected = false;
        sendEmbeddedData(mEmbedDeltas);
      } else if (!mTriggerChanges) {
        sendEmbeddedData(mEmbedDeltas);
      }
    }
    mEmbedInterval100msStepsFireCounter--;
  }
  mSignalThreadActive = false;
  LOGGER(true, LOGG_NOTIFY, "signalWorker exit")
}

void EFPSignalSend::startSignalWorker() {
  mThreadRunSignal = true;
  std::thread(std::bind(&EFPSignalSend::signalWorker, this)).detach();
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Receiver
//
//
//---------------------------------------------------------------------------------------------------------------------

EFPSignalReceive::EFPSignalReceive(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster) : ElasticFrameProtocolReceiver(bucketTimeoutMaster,holTimeoutMaster){
  ElasticFrameProtocolReceiver::receiveCallback = std::bind(&EFPSignalReceive::gotData, this, std::placeholders::_1);
  LOGGER(true, LOGG_NOTIFY, "EFPSignalReceive construct")
}

EFPSignalReceive::~EFPSignalReceive() {
  LOGGER(true, LOGG_NOTIFY, "EFPSignalReceive destruct")
}

uint32_t EFPSignalReceive::signalVersion() {
  return EFP_SIGNAL_VERSION;
}

ElasticFrameMessages EFPSignalReceive::getStreamInformationJSON(uint8_t *data,
                                                                size_t size,
                                                                std::unique_ptr<EFPSignalReceiveData> &rParsedData) {
  bool lJsonOK = true;
  std::string lContent((const char *) data, (const char *) data + size);
  json lJson, lJError;

  try {
    lJson = json::parse(lContent.c_str());
  } catch (const std::exception &e) {
    LOGGER(true, LOGG_ERROR, "Error reading json data -> " << e.what())
    return ElasticFrameMessages::dataNotJSON;
  }

  rParsedData->mSignalVersion = EFPGetContentForKey<uint32_t>("efpsignalversion_u32", lJson, lJError, lJsonOK);
  if (!lJsonOK)
    return ElasticFrameMessages::noDataForKey;
  rParsedData->mStreamVersion = EFPGetContentForKey<uint32_t>("efpstreamversion_u32", lJson, lJError, lJsonOK);
  if (!lJsonOK)
    return ElasticFrameMessages::noDataForKey;
  //auto streams = getContentForKey<std::string>("efpstreams_arr", j, jError, jsonOK);

  try {
    auto lStreams = lJson["efpstreams_arr"];

    for (auto &element : lStreams) {
      EFPStreamContent newContent(UINT32_MAX);
      EFPSignalExtraktValuesForKeyV1(newContent, element, lJError, lJsonOK);
      if (lJsonOK) {
        rParsedData->mContentList.push_back(newContent);
      } else {
        LOGGER(true, LOGG_ERROR, "ERROR parsing EFPStreamContent")
        lJsonOK = true;
      }
    }
  } catch (const std::exception &e) {
    LOGGER(true, LOGG_ERROR, "Error reading json data -> " << e.what())
    return ElasticFrameMessages::noDataForKey;
  }
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalReceive::getStreamInformationData(uint8_t *data,
                                                                size_t size,
                                                                std::unique_ptr<EFPSignalReceiveData> &parsedData) {

  if (size < sizeof(EFPStreamContent::BinaryHeaderV1)) {
    return ElasticFrameMessages::lessDataThanExpected;
  }

  EFPStreamContent::BinaryHeaderV1 lHeaderV1 = *(EFPStreamContent::BinaryHeaderV1 *) data;
  if (lHeaderV1.mEFPSignalversion > EFP_SIGNAL_VERSION) {
    return ElasticFrameMessages::tooHighversion;
  }
  if (lHeaderV1.mHeaderVersion == 1) {
    parsedData->mSignalVersion = lHeaderV1.mEFPSignalversion;
    parsedData->mStreamVersion = lHeaderV1.mEFPStreamVersion;
    uint8_t *lAllDataPtr = data + sizeof(EFPStreamContent::BinaryHeaderV1);
    for (int x = 0; x < lHeaderV1.mNumBlocks; x++) {
      EFPStreamContent newContent(UINT32_MAX);
      std::memcpy(&newContent.mVariables, lAllDataPtr, sizeof(newContent.mVariables));
      lAllDataPtr += sizeof(EFPStreamContent::Variables);
      parsedData->mContentList.push_back(newContent);
    }
  } else {
    return ElasticFrameMessages::versionNotSupported;
  }
  return ElasticFrameMessages::noError;
}

ElasticFrameMessages EFPSignalReceive::actOnDMSG(uint8_t *data,
                                                 size_t size) {

  if (size < 2) {
    LOGGER(true, LOGG_ERROR, "The size of DMSG is less than 2 bytes")
    return ElasticFrameMessages::lessDataThanExpected;
  }
  std::shared_ptr<EFPSignalSend> efpSndCpy = mEFPSend;
  //We got a DMSG message. To act on it we need to have a reference to a sender.
  if (!efpSndCpy) {
    LOGGER(true, LOGG_ERROR, "No reference to sender exists")
    return ElasticFrameMessages::dmsgSourceMissing;
  }

  uint8_t dmsgID = data[0];
  if (dmsgID == 1) {
    if (size < sizeof(EFPSignalMessages::WhiteList)) return ElasticFrameMessages::lessDataThanExpected;
    EFPSignalMessages::WhiteList lWhiteListDMSG = *(EFPSignalMessages::WhiteList *)data;
    efpSndCpy->modifyContent(lWhiteListDMSG.hContent, lWhiteListDMSG.hStreamID, [](EFPStreamContent &theContent)
                                  {
                                    theContent.mWhiteListed = true;
                                  }
    );
  } else if (dmsgID == 2) {
    if (size < sizeof(EFPSignalMessages::BlackList)) return ElasticFrameMessages::lessDataThanExpected;
    EFPSignalMessages::BlackList lBlackListDMSG = *(EFPSignalMessages::BlackList *)data;
    efpSndCpy->modifyContent(lBlackListDMSG.hContent, lBlackListDMSG.hStreamID, [](EFPStreamContent &theContent)
                             {
                               theContent.mWhiteListed = false;
                             }
    );
  }
  return ElasticFrameMessages::noError;
}

void EFPSignalReceive::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
  if (packet->mDataContent == ElasticFrameContent::efpsig) {
    if (this->contentInformationCallback) {
      std::unique_ptr<EFPSignalReceiveData> lMyData = std::make_unique<EFPSignalReceiveData>();
      if (packet->mCode == EFP_CODE('J', 'S', 'O', 'N')) {
        ElasticFrameMessages lStatus = getStreamInformationJSON(packet->pFrameData, packet->mFrameSize, lMyData);
        if (lStatus != ElasticFrameMessages::noError) {
          LOGGER(true, LOGG_ERROR, "ERROR parsing EFPStreamContent JSON")
          return;
        }
      } else if (packet->mCode == EFP_CODE('D', 'A', 'T', 'A')) {
        ElasticFrameMessages lStatus = getStreamInformationData(packet->pFrameData, packet->mFrameSize, lMyData);
        if (lStatus != ElasticFrameMessages::noError) {
          LOGGER(true, LOGG_ERROR, "ERROR parsing EFPStreamContent DATA")
          return;
        }
      } else if (packet->mCode == EFP_CODE('D', 'M', 'S', 'G')) {
        ElasticFrameMessages lStatus = actOnDMSG(packet->pFrameData, packet->mFrameSize);
        return;
      } else {
        LOGGER(true, LOGG_ERROR, "Unknown EFPStreamContent")
        return;
      }
      this->contentInformationCallback(lMyData);
      return;
    } else {
      LOGGER(true, LOGG_ERROR, "contentInformationCallback not defined")
      return;
    }
  }
  if (this->receiveCallback) {
    this->receiveCallback(packet);
  }
}

