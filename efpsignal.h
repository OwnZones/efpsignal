// EFPSignal
//
// UnitX Edgeware AB 2020
//

#ifndef EFPSIGNAL__EFPSIGNAL_H
#define EFPSIGNAL__EFPSIGNAL_H

#include "json.hpp"
#include "ElasticFrameProtocol.h"

#define EFP_SIGNAL_VERSION 1 //uint32_t

//JSON parse / generate
using json = nlohmann::json;

//Global template for getting JSON value for key
template <typename T>
T EFPGetContentForKey(std::string g,json& j,json& jError, bool& jsonOK) {
  T data;
  try {
    data=j[g.c_str()];
  } catch (const std::exception& e) {
    jError.push_back(json{g.c_str(),e.what()});
    jsonOK=false;
  }
  return data;
}

//DMSG messages sent to a sender for controlling the flows
//DMSG is ONLY internal signaling meaning for OOB any protocol of your choice may be used
//DMSG is using public mathods at the sender side. A OOB protocol also do the exact same thing.
struct EFPSignalMessages {
public:
      //Whitelist the content/id
  struct WhiteList {
    uint8_t hID=1;
    uint8_t hVersion=1;
    ElasticFrameContent hContent = ElasticFrameContent::unknown;
    uint8_t hStreamID = 0;
  };
      //Blacklist the content/id
  struct BlackList {
    uint8_t hID=2;
    uint8_t hVersion=1;
    ElasticFrameContent hContent = ElasticFrameContent::unknown;
    uint8_t hStreamID = 0;
  };
      //Request steam information for the content/id
  struct RequestStreamsInformation {
    uint8_t hID=3;
    uint8_t hVersion=1;
    ElasticFrameContent hContent = ElasticFrameContent::unknown;
    uint8_t hStreamID = 0;
  };

    //Request the sender to provide all stream information
  struct RequestAllStreamsInformation {
    uint8_t hID=4;
    uint8_t hVersion=1;
  };

  //Configure senders stream information settings
  struct SetEmbedStreamInformation {
    uint8_t hID=5;
    uint8_t hVersion=1;
    uint8_t hEmbedInStream=1; //0 = Do not embed in EFP stream 1 = embed in EFP stream 2 = dont change
    uint8_t hBinaryMode=1; //0 = JSON 1 = Binary 2 = dont change
    uint8_t hTriggerChange = 0; //0 = do not trigger on change. 1 = trigger on changes 2 = dont change
    uint8_t HEmbedDeltas = 0; //0 = embed all 1 = embed only changes 2 = dont change
    uint8_t hEmbedInterval100msSteps = 10; //interval * 100ms == time between embedded stream information 2 = dont change
  };
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPStreamContent
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class EFPStreamContent
 *
 * \brief
 *
 * EFPStreamContent contains each elements content declaration
 * The 'Variables' part is the body sent using JSON or Binary
 * The JSON key name is created by removing the leading m (member) then lower case all characters and
 * adding trailing _(type)
 * value is _(val) where u == unsigned i == signed followed by number of bits. uint32_t == _u32
 * float is _flt
 * double is _dbl
 * bool is _bol
 * array is _arr
 * string is _str
 * dictionary _dct
 *
 */
class EFPStreamContent {
public:

  ///Constructor passing ttl for the content from when created in ms
  EFPStreamContent(int32_t ttlms) {
    mTimeToLivems = ttlms;
    mCurrentTimeToLivems = ttlms;
  }

  ///Destructor
 // virtual ~EFPStreamContent(){

 // };

  /**
   * is the content still allive or has time passed
   *
   * @param number of ms passed since last time we checked.
   * @return true if still alive, false if not.
   */
  bool isStillAlive(int32_t msPassed) {
    mCurrentTimeToLivems -= msPassed;
    if (mCurrentTimeToLivems <= 0 ) {
      return false;
    }
    return true;
  }

  /**
   * Reset the TTL
   *
   */
  void resetTTL() {
    mCurrentTimeToLivems = mTimeToLivems;
  }

  /// If the content is whitelisted EFPSignal lets this content trough the filter else it's blocking this content.
  bool mWhiteListed = false;

  /// The descriptive body for the content (Manifest describing a single EFP stream)
  struct Variables {
    //General part

    /// ElasticFrameContent as described in ElasticFrameProtocol
    ElasticFrameContent mGFrameContent = ElasticFrameContent::unknown;
    /// ElasticFrameProtocols EFP_ID meaning the streamID associated with this content
    uint8_t mGStreamID = 0;
    /// 0 = Same information as last time
    /// 1 = item added
    /// 2 = item changed
    /// 3 = item deleted
    /// Setting this parameter to anything will be overridden by EFPSignal
    uint8_t mGChanged = 0;
    /// If this content is part of a protection group the protection group ID is noted else 0
    /// A protection group may only contain the same data-type
    uint8_t mGProtectionGroupID = 0;
    /// If this content is part of a sync-group the sync-group group ID is noted else 0
    uint8_t mGSyncGroupID = 0;
    /// The priority of the content used in protection groups
    /// If the priority is set same for two content descriptions it's assumed the content comes from the same source
    uint8_t mGPriority = 0;
    /// Notify here is not currently implemented
    /// Notify here contains a UTC based time used by the not implemented notification messages
    uint64_t mGNotifyHere = 0;
    /// PTS & DTS timebase
    uint32_t mGPTSDTSBase = 90000;

    //Video part
    /// The numerator part of the frame rate
    uint32_t mVFrameRateNum = 0;
    /// The denominator part of the frame rate
    uint32_t mVFrameRateDen = 0;
    /// The width of the video content
    uint32_t mVWidth = 0;
    /// The height of the video content
    uint32_t mVHeight = 0;
    /// Number bits per second for the video
    uint32_t mVBitsPerSec = 0;

    //Audio part
    /// Audio frequency in Hz
    uint32_t mAFreq = 0;
    /// Number of audio channels
    uint32_t mANoChannels = 0;
    /// Channel mapping structure
    /// 0 = unknown
    /// 1 = L,R
    /// 2 = R,L
    /// 3 = Lf,Rf,Lr,Rr,C,LFE
    uint32_t mAChannelMapping = 0;
    /// Number bits per second for the audio
    uint32_t mABitsPerSec = 0;

    //Text part
    /// The name is a combination of an ISO 639 two-letter lowercase culture code associated with
    /// a language and an ISO 3166 two-letter uppercase subculture code associated with a country or region.
    // "xx-XX"
    char mTLanguage[6]="";
    /// Text configuration
    char mTextConfig[10]="";

    //auX part
    /// Free to use aux type
    uint32_t mXType = 0;
    /// Free to use aux string
    char mXString[10] = "";
    /// Free to use aux value
    uint32_t mXValue = 0;
  } mVariables;


  /**
   * BinaryHeaderV1 version 1 header used when sending the variables as binary data.
   *
   */
  struct BinaryHeaderV1 {
  public:
    const uint16_t mHeaderVersion = 1; //This entry may not move in new versions
    const uint32_t mEFPSignalversion = EFP_SIGNAL_VERSION;
    uint32_t mEFPStreamVersion = 0;
    uint32_t mNumBlocks = 0;
    const uint32_t mVariablesSize = sizeof(Variables);
  };

private:
  int32_t mTimeToLivems; // This content TTL
  int32_t mCurrentTimeToLivems; // The current TTL
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Sender
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class EFPSignalSend
 *
 * \brief
 *
 * EFPSignalSend is subclassing ElasticFrameProtocolSender and overloads 'the pack and send' methods
 * EFPSignalSend registers and filters the elementary data based on user settings
 *
 * \author UnitX
 *
 * Contact: bitbucket:andersced
 * or-> anders dot cedronius at edgeware dot tv
 *
 */
class EFPSignalSend : public ElasticFrameProtocolSender {
public:

  //For the future ABR clean cut implementation This class is WIP
  class SyncGroup {
  public:
    class SyncItem{
    public:
      ElasticFrameContent mFrameContent = ElasticFrameContent::unknown;
      uint8_t mStreamID = 0;
      bool mActive = false;
      std::vector<uint8_t> data;
    };
    uint8_t mSyncGroupID = 0;
    std::vector<SyncItem> mSyncGroup;

    void putData(ElasticFrameContent frameContent, uint8_t streamID, std::vector<uint8_t> &data) {
      for (auto &rItem: mSyncGroup) {
        if (rItem.mFrameContent == frameContent && rItem.mStreamID == streamID) {
          rItem.data.insert(rItem.data.end(),data.begin(),data.end()); //Realloc and copy
          break;
        }
      }
    }

    void getData(ElasticFrameContent frameContent, uint8_t streamID, std::vector<uint8_t> &data) {
      for (auto &rItem: mSyncGroup) {
        if (rItem.mFrameContent == frameContent && rItem.mStreamID == streamID) {
          data = rItem.data; //copy of data wil happen here
          break;
        }
      }
    }
  };

  /**
   * Constructor
   *
   * @param setMTU is the MTU used by ElasticFrameProtocolSender
   * @param garbageCollectms is the number of ms to set the TTL to when detecting new content.
   */
  explicit EFPSignalSend(uint16_t setMTU, uint32_t garbageCollectms);

  ///Destructor
  ~EFPSignalSend() override;

  /**
   * packAndSend overrides the ElasticFrameProtocolSender virtual method. then it calls ElasticFrameProtocolSender method
   * after declaring new content or filtering not white labeled content.
   *
   * for parameters please see the ElasticFrameProtocolSender description
   */
  ElasticFrameMessages
  packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent frameContent, uint64_t pts, uint64_t dts,
              uint32_t code,
              uint8_t streamID, uint8_t flags) override;

  /**
   * packAndSendFromPtr overrides the ElasticFrameProtocolSender virtual method. then it calls ElasticFrameProtocolSender method
   * after declaring new content or filtering not white labeled content.
   *
   * for parameters please see the ElasticFrameProtocolSender description
   */
  ElasticFrameMessages
  packAndSendFromPtr(const uint8_t* pPacket, size_t packetSize, ElasticFrameContent frameContent, uint64_t pts, uint64_t dts,
                     uint32_t code, uint8_t streamID, uint8_t flags) override;


  /**
   * Clears all registered content
   *
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages clearAllContent();

  /**
   * Adds content as specified in EFPStreamContent
   *
   * @param rStreamContent a EFPStreamContent
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages addContent(EFPStreamContent &rStreamContent);

  /**
   * Deletes contend based on stream id and content type
   *
   * @param frameContent ElasticFrameContent-type
   * @param streamID EFP stream-id
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages deleteContent(ElasticFrameContent frameContent, uint8_t streamID);

  /**
   * getContent gets content based on stream id and content type
   *
   * @param rStreamContent a reference to EFPStreamContent. Will be populated if id and type is found.
   * @param frameContent ElasticFrameContent-type
   * @param streamID EFP stream-id
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages getContent(EFPStreamContent &rStreamContent, ElasticFrameContent frameContent, uint8_t streamID);

  /**
   * modifyContent content based on stream id and content type
   *
   * @param frameContent ElasticFrameContent-type
   * @param streamID EFP stream-id
   * @param function/lambda passed for modifying the content.
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages modifyContent(ElasticFrameContent frameContent, uint8_t streamID, std::function<void(EFPStreamContent &)> function);

  /**
   * generateJSONStreamInfoFromData is generating JSON formated data from EFPStreamContent
   *
   * @param rJsonContent EFPStreamContent-type-json
   * @param rStreamContent EFPStreamContent-type-data
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages generateJSONStreamInfoFromData(json &rJsonContent, EFPStreamContent &rStreamContent);

  /**
   * generateDataStreamInfoFromJSON is generating EFPStreamContent from JSON formated EFPStreamContent
   *
   * @param rStreamContent EFPStreamContent-type-data
   * @param rJsonContent EFPStreamContent-type-json
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages generateDataStreamInfoFromJSON(EFPStreamContent &rStreamContent, json &rJsonContent);

  /**
   * generateAllStreamInfoJSON is generating JSON formated EFPStreamContent from all declared EFP streams
   *
   * @param rJsonContent EFPStreamContent-type-json
   * @param deltasOnly if true, only changes will be enterd into the JSON data
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages generateAllStreamInfoJSON(json &rJsonContent, bool deltasOnly);

  /**
   * generateAllStreamInfoData is generating JSON formated EFPStreamContent from all declared EFP streams
   *
   * @param rStreamContentData vector of EFPStreamContent-type-data + version_x header
   * @param deltasOnly if true, only changes will be enterd into the JSON data
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages generateAllStreamInfoData(std::unique_ptr<std::vector<uint8_t>> &rStreamContentData, bool deltasOnly);

  /**
   * sendEmbeddedData embed stream information into EFP
   *
   * @param deltasOnly if true, only changes will be enterd into the JSON data
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages sendEmbeddedData(bool deltasOnly);

  /**
   * signalVersion returns EFP_SIGNAL_VERSION
   *
   * @return EFP_SIGNAL_VERSION
   */
  uint32_t signalVersion();

  ///Callback for filling out stream declarations if mAutoRegister is set to true and new content is detected
  std::function<bool(EFPStreamContent& content)> declareContentCallback = nullptr;
  ///if mEmbedInStream is false this callback is triggered instead for OOB usage of declarations
  std::function<void(std::unique_ptr<std::vector<uint8_t>> &rStreamContentData, json &rJsonContent)> declarationCallback = nullptr;

  ///Delete copy and move constructors and assign operators
  EFPSignalSend(EFPSignalSend const &) = delete;              // Copy construct
  EFPSignalSend(EFPSignalSend &&) = delete;                   // Move construct
  EFPSignalSend &operator=(EFPSignalSend const &) = delete;   // Copy assign
  EFPSignalSend &operator=(EFPSignalSend &&) = delete;        // Move assign

  //Setings

  /// Auto register new content using declareContentCallback
  bool mAutoRegister = true;

  /// Embed declaration in EFP as a stream
  bool mEmbedInStream = true;
  /// Generate a binary struct (true) or JSON (false)
  bool mBinaryMode = false;

  ///100ms intervals between automatic generation of current information
  uint32_t mEmbedInterval100msSteps = 1;

  ///Trigger on changes only (true) or trigger on timer (false)
  bool mTriggerChanges = false;

  /// Only generate deltas compared to previous declaration generation
  bool mEmbedDeltas = false;

  /// Time in ms before garbage collecting stream information since last seen.
  int32_t mGarbageCollectms = 0;

private:
  void startSignalWorker(); //Start the internal thread
  void signalWorker(); //The internal thread
  ElasticFrameMessages signalFilter(ElasticFrameContent dataContent, uint8_t streamID, uint32_t *dataMessage); //The EFP stream filter

  bool mChangeDetected = false;  //Change?
  uint32_t mEmbedInterval100msStepsFireCounter = 0; //Time counter
  uint32_t mEFPStreamListVersion = 1; //The current list version
  uint32_t mOldEFPStreamListVersion = 1; //The previous version
  bool mIsKnown[UINT8_MAX+1][UINT8_MAX+1] = {false}; //Array of known EFP streams
  std::mutex mStreamListMtx; //Mutex protecting the stream lists
  std::vector<std::vector<EFPStreamContent>> mEFPStreamLists; //The current list of items.
  std::vector<SyncGroup> mSyncGroups; //WIP
  std::atomic_bool mThreadRunSignal; //Internal thread signal
  std::atomic_bool mSignalThreadActive; //Internal thread signal
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// EFPSignal Receiver
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class EFPStreamContent
 *
 * \brief
 *
 * ElasticFrameProtocolSender can be used to frame elementary streams to EFP fragments for transport over any network technology
 *
 * \author UnitX
 *
 * Contact: bitbucket:andersced
 *
 */
class EFPSignalReceive : public ElasticFrameProtocolReceiver {
public:

  ///Class containing the stream content
  class EFPSignalReceiveData {
  public:
    uint32_t mSignalVersion = 0;
    uint32_t mStreamVersion = 0;
    std::vector<EFPStreamContent> mContentList;
  };

  ///Constructor
  explicit EFPSignalReceive(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster);

  ///Destructor
  ~EFPSignalReceive() override;

  /**
   * getStreamInformationJSON is generating EFPSignalReceiveData from a JSON formatted data block
   *
   * @param data pointer to the start of data
   * @param size of the data block
   * @param parsedData is the data output
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages getStreamInformationJSON(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);

  /**
   * getStreamInformationData is generating EFPSignalReceiveData from a binary formatted data block
   *
   * @param data pointer to the start of data
   * @param size of the data block
   * @param parsedData is the data output
   * @return ElasticFrameMessages
   */
  ElasticFrameMessages getStreamInformationData(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);

  /**
   * setSenderBinding is used in duplex mode for DMSG to control a sender from the received DMSG messages.
   *
   * @brief may only be used if the original EFPSignalSend is a shared pointer.
   *
   * @param sender a shared pointer to the EFPSignalSend in use.
   */
  void setSenderBinding(std::shared_ptr<EFPSignalSend> sender) {mEFPSend = sender;}

  /**
   * signalVersion returns EFP_SIGNAL_VERSION
   *
   * @return EFP_SIGNAL_VERSION
   */
  uint32_t signalVersion();

  ///Overrides the EFP receiveCallback for parsing EFPSignal messages and declarations.
  std::function<void(pFramePtr &rPacket)> receiveCallback = nullptr;
  ///Callback for embedded content declarations (not nullable)
  std::function<void(std::unique_ptr<EFPSignalReceiveData>& data)> contentInformationCallback = nullptr;

  ///Delete copy and move constructors and assign operators
  EFPSignalReceive(EFPSignalReceive const &) = delete;              // Copy construct
  EFPSignalReceive(EFPSignalReceive &&) = delete;                   // Move construct
  EFPSignalReceive &operator=(EFPSignalReceive const &) = delete;   // Copy assign
  EFPSignalReceive &operator=(EFPSignalReceive &&) = delete;        // Move assign

private:
  std::shared_ptr<EFPSignalSend> mEFPSend = nullptr; //If duplex mode the sender to controll from the receiver DMSG's
  void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet); //The receiver got data to parse if it's EFPSignal
  ElasticFrameMessages actOnDMSG(uint8_t *data, size_t size); //DMSG executor/parser
};

/// Helper class for creating a duplex EFPSignal group (Tx/Rx) and maintaining its lifecycle.
class EFPSignalDuplex {
public:
  /// Constructor. Bucket time out, head of line blocking time out, mtu and EFP signal time out
  explicit EFPSignalDuplex(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster, uint16_t setMTU, uint32_t garbageCollectms){
    mEFPSend = std::make_shared<EFPSignalSend>(setMTU,garbageCollectms);
    mEFPReceive = std::make_shared<EFPSignalReceive>(bucketTimeoutMaster,holTimeoutMaster);
    mEFPReceive->setSenderBinding(mEFPSend);
  }

  /// Destructor. Deleate all of my owners in the correct order.
  ~EFPSignalDuplex() {
    mEFPReceive->setSenderBinding(nullptr);
    mEFPReceive = nullptr;
    mEFPSend = nullptr;
  }
  std::shared_ptr<EFPSignalSend> mEFPSend = nullptr;
  std::shared_ptr<EFPSignalReceive> mEFPReceive = nullptr;
};

#endif //EFPSIGNAL__EFPSIGNAL_H

