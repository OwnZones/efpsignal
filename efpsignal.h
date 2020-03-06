//
// Created by Anders Cedronius on 2020-03-06.
//

#include "ElasticFrameProtocol.h"

#ifndef EFPSIGNAL__EFPSIGNAL_H
#define EFPSIGNAL__EFPSIGNAL_H

class EFPSignalSend : public ElasticFrameProtocolSender {
public:

  ///Constructor
  explicit EFPSignalSend(uint16_t setMTU, uint32_t garbageCollectms ) : ElasticFrameProtocolSender(setMTU){
    mThreadRunSignal = true;
    mNewEntry = false;
    efpStreamLists.reserve(256);
    mGarbageCollectms = garbageCollectms;
    startSignalWorker();
  };

  ///Destructor
  virtual ~EFPSignalSend();

  ElasticFrameMessages
  SignalPackAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
              uint32_t code,
              uint8_t streamID, uint8_t flags);

  ElasticFrameMessages
  SignalPackAndSendFromPtr(const uint8_t* pPacket, size_t packetSize, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                     uint32_t code, uint8_t streamID, uint8_t flags);

  ///Delete copy and move constructors and assign operators
  EFPSignalSend(EFPSignalSend const &) = delete;              // Copy construct
  EFPSignalSend(EFPSignalSend &&) = delete;                   // Move construct
  EFPSignalSend &operator=(EFPSignalSend const &) = delete;   // Copy assign
  EFPSignalSend &operator=(EFPSignalSend &&) = delete;        // Move assign

  class EFPStreamContent {
  public:
    explicit EFPStreamContent(int32_t ttlms) {
      mTimeToLivems = ttlms;
      mCurrentTimeToLivems = ttlms;
    }
    virtual ~EFPStreamContent();
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

    //General part
    std::string mGDescription ="";
    ElasticFrameContent mGFrameContent = ElasticFrameContent::unknown;
    uint8_t mGStreamID = 0;
    uint8_t mGProtectionGroupID = 0;
    uint8_t mGSyncGroupID = 0;
    uint8_t mGPriority = 0;

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

  private:
    int32_t mTimeToLivems;
    std::atomic_int32_t mCurrentTimeToLivems;
  };

  std::function<void(EFPStreamContent* content)> declareContentCallback = nullptr;
protected:
  int32_t mGarbageCollectms = 0;

private:
  void startSignalWorker();
  void signalWorker();

  bool isKnown[256][256] = {false};
  std::vector<std::vector<EFPStreamContent>> efpStreamLists;
  std::atomic_bool mThreadRunSignal;
  std::atomic_bool mSignalThreadActive;
  std::atomic_bool mNewEntry;
};

#endif //EFPSIGNAL__EFPSIGNAL_H
