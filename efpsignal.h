// EFPSignal
//
// UnitX Edgeware AB 2020
//

// Reflection regarding



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

  bool mWhiteListed = false;


  struct Variables {
    //General part
    std::string mGDescription = "";
    ElasticFrameContent mGFrameContent = ElasticFrameContent::unknown;
    uint8_t mGStreamID = 0;
    uint8_t mGChanged = 0;
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
    uint32_t mAFreq = 0;
    uint32_t mANoChannels = 0;
    uint32_t mAChannelMapping = 0;
    uint32_t mABitsPerSec = 0;

    //Text part
    std::string mTLanguage = "";
    std::string mTextConfig = "";

    //auX part
    uint32_t mXType = 0;
    std::string mXString = "";
    uint32_t mXValue = 0;
  } mVariables;

  struct BinaryHeaderV1 {
  public:
    const uint16_t mHeaderVersion = 1; //This entry may not move in new versions
    const uint32_t mEFPSignalversion = EFP_SIGNAL_VERSION;
    uint32_t mEFPStreamVersion = 0;
    uint32_t mNumBlocks = 0;
    const uint32_t mVariablesSize = sizeof(Variables);
  };

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

  class SyncGroup {
  public:
    class SyncItem{
    public:
      uint8_t mStreamID = 0;
      std::vector<uint8_t> data;
    };
    uint8_t mSyncGroupID = 0;
    std::vector<SyncItem> items;
  };

  ///Constructor
  explicit EFPSignalSend(uint16_t setMTU, uint32_t garbageCollectms);

  ///Destructor
  ~EFPSignalSend() override;

  ElasticFrameMessages
  packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent frameContent, uint64_t pts, uint64_t dts,
              uint32_t code,
              uint8_t streamID, uint8_t flags) override;

  ElasticFrameMessages
  packAndSendFromPtr(const uint8_t* pPacket, size_t packetSize, ElasticFrameContent frameContent, uint64_t pts, uint64_t dts,
                     uint32_t code, uint8_t streamID, uint8_t flags) override;

  ElasticFrameMessages clearAllContent();
  ElasticFrameMessages addContent(EFPStreamContent &rStreamContent);
  ElasticFrameMessages deleteContent(ElasticFrameContent frameContent, uint8_t streamID);
  ElasticFrameMessages getContent(EFPStreamContent &rStreamContent, ElasticFrameContent frameContent, uint8_t streamID);
  ElasticFrameMessages modifyContent(ElasticFrameContent frameContent, uint8_t streamID, std::function<void(EFPStreamContent &)> function);
  ElasticFrameMessages generateJSONStreamInfoFromData(json &rJsonContent, EFPStreamContent &rStreamContent);
  ElasticFrameMessages generateDataStreamInfoFromJSON(EFPStreamContent &rStreamContent, json &rJsonContent);
  ElasticFrameMessages generateAllStreamInfoJSON(json &rJsonContent, bool deltasOnly);
  ElasticFrameMessages generateAllStreamInfoData(std::unique_ptr<std::vector<uint8_t>> &rStreamContentData, bool deltasOnly);
  ElasticFrameMessages sendEmbeddedData(bool deltasOnly);
  uint32_t signalVersion();

  std::function<bool(EFPStreamContent& content)> declareContentCallback = nullptr;
  std::function<void(std::unique_ptr<std::vector<uint8_t>> &rStreamContentData, json &rJsonContent)> declarationCallback = nullptr;

  ///Delete copy and move constructors and assign operators
  EFPSignalSend(EFPSignalSend const &) = delete;              // Copy construct
  EFPSignalSend(EFPSignalSend &&) = delete;                   // Move construct
  EFPSignalSend &operator=(EFPSignalSend const &) = delete;   // Copy assign
  EFPSignalSend &operator=(EFPSignalSend &&) = delete;        // Move assign

  //Setings
  bool mAutoRegister = true;

  bool mEmbedInStream = true;
  bool mBinaryMode = false;

  uint32_t mEmbedInterval100msSteps = 1;
  bool mTriggerChanges = false;
  bool mEmbedDeltas = false;

protected:
  int32_t mGarbageCollectms = 0;

private:
  void startSignalWorker();
  void signalWorker();
  ElasticFrameMessages signalFilter(ElasticFrameContent dataContent, uint8_t streamID, uint32_t *dataMessage);

  bool mChangeDetected = false;
  uint32_t mEmbedInterval100msStepsFireCounter = 0;
  uint32_t mEFPStreamListVersion = 1;
  uint32_t mOldEFPStreamListVersion = 1;
  bool mIsKnown[UINT8_MAX+1][UINT8_MAX+1] = {false};
  std::mutex mStreamListMtx; //Mutex protecting the stream lists
  std::vector<std::vector<EFPStreamContent>> mEFPStreamLists;
  std::vector<SyncGroup> mSyncGroups;
  std::atomic_bool mThreadRunSignal{};
  std::atomic_bool mSignalThreadActive{};
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
    uint32_t mSignalVersion = 0;
    uint32_t mStreamVersion = 0;
    std::vector<EFPStreamContent> mContentList;
  };

  ///Constructor
  explicit EFPSignalReceive(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster );

  ///Destructor
  ~EFPSignalReceive() override;

  ElasticFrameMessages getStreamInformationJSON(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);
  ElasticFrameMessages getStreamInformationData(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);
  uint32_t signalVersion();

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
