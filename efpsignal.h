//
// Created by Anders Cedronius on 2020-03-06.
//



#ifndef EFPSIGNAL__EFPSIGNAL_H
#define EFPSIGNAL__EFPSIGNAL_H

#include "json.hpp"
#include "ElasticFrameProtocol.h"

//A uint32_t value
#define EFP_SIGNAL_VERSION 1

using json = nlohmann::json;

//Global template for getting value for key
template <typename T>
T getContentForKey(std::string g,json& j,json& jError, bool& jsonOK) {
  T data;
  try {
    data=j[g.c_str()];
  } catch (const std::exception& e) {
    jError.push_back(json{g.c_str(),e.what()});
    jsonOK=false;
  }
  return data;
}

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPStreamContent
//
//
//---------------------------------------------------------------------------------------------------------------------

class EFPStreamContent {
public:

  EFPStreamContent(int32_t ttlms) {
    mTimeToLivems = ttlms;
    mCurrentTimeToLivems = ttlms;
  }

  virtual ~EFPStreamContent(){

  };

  bool isStillAlive(int32_t msPassed) {
    mCurrentTimeToLivems -= msPassed;
    if (mCurrentTimeToLivems <= 0 ) {
      return false;
    }
    return true;
  }

  void resetTTL() {
    mCurrentTimeToLivems = mTimeToLivems;
  }

  struct Variables {
    uint32_t binaryVersion = EFP_SIGNAL_VERSION;
    //General part
    std::string mGDescription = "";
    ElasticFrameContent mGFrameContent = ElasticFrameContent::unknown;
    uint8_t mGStreamID = 0;
    uint8_t mGProtectionGroupID = 0;
    uint8_t mGSyncGroupID = 0;
    uint8_t mGPriority = 0;
    uint64_t mGNotifyHere = 0;

    //Video part
    uint32_t mVFrameRateNum = 0;
    uint32_t mVFrameRateDen = 0;
    uint32_t mVWidth = 0;
    uint32_t mVHeight = 0;
    uint32_t mVBitsPerSec = 0;

    //Audio part
    uint32_t mAFreqNum = 0;
    uint32_t mAFreqDen = 0;
    uint32_t mANoChannels = 0;
    uint32_t mAChannelMapping = 0;
    uint32_t mABitsPerSec = 0;

    //Text part
    uint32_t mTTextType = 0;
    std::string mTLanguage = "";

    //auX part
    uint32_t mXType = 0;
    std::string mXString = "";
    uint32_t mXValue = 0;
  } mVariables;

private:
  int32_t mTimeToLivems;
  int32_t mCurrentTimeToLivems;
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Sender
//
//
//---------------------------------------------------------------------------------------------------------------------

class EFPSignalSend : public ElasticFrameProtocolSender {
public:

  ///Constructor
  explicit EFPSignalSend(uint16_t setMTU, uint32_t garbageCollectms ) : ElasticFrameProtocolSender(setMTU){
    mThreadRunSignal = true;
    mEFPStreamLists.reserve(256);
    mGarbageCollectms = garbageCollectms;
    startSignalWorker();
  };

  ///Destructor
  virtual ~EFPSignalSend();

  ElasticFrameMessages
  packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
              uint32_t code,
              uint8_t streamID, uint8_t flags);

  ElasticFrameMessages
  packAndSendFromPtr(const uint8_t* pPacket, size_t packetSize, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                     uint32_t code, uint8_t streamID, uint8_t flags);

  ElasticFrameMessages registerContent(EFPStreamContent &content);
  ElasticFrameMessages deleteContent(ElasticFrameContent dataContent, uint8_t streamID);
  EFPStreamContent getContent(ElasticFrameContent dataContent, uint8_t streamID);
  json generateStreamInfo(EFPStreamContent &content);
  json generateAllStreamInfo();
  uint32_t signalVersion();

  std::function<void(EFPStreamContent* content)> declareContentCallback = nullptr;

  ///Delete copy and move constructors and assign operators
  EFPSignalSend(EFPSignalSend const &) = delete;              // Copy construct
  EFPSignalSend(EFPSignalSend &&) = delete;                   // Move construct
  EFPSignalSend &operator=(EFPSignalSend const &) = delete;   // Copy assign
  EFPSignalSend &operator=(EFPSignalSend &&) = delete;        // Move assign

  //Setings
  bool mDropUnknown = false;
  bool mAutoRegister = true;
  bool mEmbeddInStream = true;
  bool mEmbeddOnlyChanges = false;
  bool mChangeDetected = false;
  bool mEmbeddBinary = false;
  uint32_t mEmbedInterval100msSteps = 1;

protected:
  int32_t mGarbageCollectms = 0;

private:
  void startSignalWorker();
  void signalWorker();
  ElasticFrameMessages signalFilter(ElasticFrameContent dataContent, uint8_t streamID);


  uint32_t mEmbedInterval100msStepsFireCounter = 0;
  uint32_t mEFPStreamListVersion = 1;
  uint32_t mOldEFPStreamListVersion = 1;
  bool mIsKnown[UINT8_MAX+1][UINT8_MAX+1] = {false};
  std::mutex mStreamListMtx; //Mutex protecting the stream lists
  std::vector<std::vector<EFPStreamContent>> mEFPStreamLists;
  std::atomic_bool mThreadRunSignal;
  std::atomic_bool mSignalThreadActive;
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Receiver
//
//
//---------------------------------------------------------------------------------------------------------------------

class EFPSignalReceive : public ElasticFrameProtocolReceiver {
public:

  class EFPSignalReceiveData {
  public:
    uint32_t signalVersion = 0;
    uint32_t streamVersion = 0;
    std::vector<EFPStreamContent> contentList;
  };

  ///Constructor
  explicit EFPSignalReceive();

  ///Destructor
  virtual ~EFPSignalReceive();

  uint32_t signalVersion();
  ElasticFrameMessages getStreamInformation(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);

  std::function<void(pFramePtr &rPacket)> receiveCallback = nullptr;
  std::function<void(std::unique_ptr<EFPSignalReceiveData>& data)> contentInformationCallback = nullptr;

  ///Delete copy and move constructors and assign operators
  EFPSignalReceive(EFPSignalReceive const &) = delete;              // Copy construct
  EFPSignalReceive(EFPSignalReceive &&) = delete;                   // Move construct
  EFPSignalReceive &operator=(EFPSignalReceive const &) = delete;   // Copy assign
  EFPSignalReceive &operator=(EFPSignalReceive &&) = delete;        // Move assign

private:
  void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet);
};

#endif //EFPSIGNAL__EFPSIGNAL_H

/*
 * Version
 *
 */
