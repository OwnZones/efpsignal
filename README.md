![alt text](efpsignal.jpg)

# EFPSignal

The EFPSignal is a control plane used by [ElasticFrameProtocol](https://github.com/agilecontent/efp).

```
--------------------------------------------------------- ---     /\
| Data type L | Data type L | Data type F | Data type Q |     |  /  \
---------------------------------------------------------  E  |   ||
|                     EFPSignal (Wraps EFP)             |  F  |   ||
|     ---------------------------------------------     |  P  |   ||
|     |                                            |    |  S  |   ||
|     |              ElasticFrameProtocol          |    |  i  |   ||
|     |                                            |    |  g  |   ||
|      ---------------------------------------------    |  n  |   ||
|                                                       |  a  |   ||
---------------------------------------------------------  l  |   ||
| Network layer: UDP, TCP, SRT, RIST, Zixi, SCTP, aso.  |     |  \  /
--------------------------------------------------------- ---     \/

```

EFPSignal wraps around ElasticFrameProtocol to intercept and notify the user about the dataflow between the data producers / consumers and the underlying network layer. The signaling can be Out-Of-Band or In-Band. The illustration tries to illustrate this.


Please read -> [**EFPSignal**](https://github.com/agilecontent/efpsignal/blob/master/docs/EFPSignalPlugin.pdf) for more information.


## Installation

Requires cmake version >= **3.10** and **C++17**

**Release:**

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

***Debug:***

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

Output: 

**(platform specific)efpsignal.(platform specific)** (Linux/MacOS -> libefpsignal.a)

The static EFPSignal library
 
**efpsignalsimplextests**

*efpsignalsimplextests* (executable) runs trough the simplex unit tests and returns EXIT_SUCESS if all unit tests pass.

**efpsignalduplextests**

*efpsignalduplextests* (executable) runs trough the duplex unit tests and returns EXIT_SUCESS if all unit tests pass.

See the source code for examples on how to use EFPSignal.


## Usage

The EFPSignal class subclassing ElasticFrameProtocol. Use EFPSignal just as you would use EFP with the addition of the parameters and callbacks of EFPSignal as described below.

**Sender:**

```cpp
// Create your sender. Just as you do with EFP but now with the addition of 
// a parameter (5000 in this example).
// This parameter is defining the time-out (ms) of content auto-declared. 

EFPSignalSend myEFPSignalSend(MTU, 5000);

// Register your callback
myEFPSignalSend.declareContentCallback = std::bind(&declareContent, std::placeholders::_1);

// This callback is triggered when new content is seen 

// You can manually declare content by using the method
ElasticFrameMessages registerContent(EFPStreamContent &content);

// You can manually delete content by using the method
ElasticFrameMessages deleteContent(ElasticFrameContent dataContent, uint8_t streamID);

// You can manually get content from the content list by
EFPStreamContent getContent(ElasticFrameContent dataContent, uint8_t streamID);

// You can convert a single content ID to JSON by
json generateStreamInfo(EFPStreamContent &content);

// You can get the full list of all content declared as JSON by
json generateAllStreamInfo();

// Get the EFPSignal version by
uint32_t signalVersion();

//
//EFPSignal Setings
//

// Drop unknown data
//false = do not drop
//true = drop
bool mDropUnknown;

//Auto register unknown data (callback is triggered if unknown data appears)
//false = register manually
//true = auto-register
bool mAutoRegister;

// Put current declaration in EFP-Stream id 0
//false = do not embed in stream
//true = embed in stream
bool mEmbeddInStream;

// Only send declaration if changes are seen 
//false = send all data
//true = send only changes
bool mEmbeddOnlyChanges;

//Embed binary else JSON format is selected
//false = JSON format used
//true = binary format used
bool mEmbeddBinary;

//Declaration interval
//number = 100ms * the number is used as interval for sending declarations.
uint32_t mEmbedInterval100msSteps = number;


```

**Reciever:**

```cpp

// Create your reciever. Just as you do with EFP.
// additional optional parameters exists such as context and 
// configuring run to completion 
EFPSignalReceive myEFPSignalReceive(50,20);


// Register your callback where you recieve in-band signaling 
//For OOB this callback is not needed
myEFPSignalReceive.contentInformationCallback = std::bind(&gotContentInformation, std::placeholders::_1);

//If OOB is used then the helper method can be used to interprete the JSON data
ElasticFrameMessages getStreamInformation(uint8_t *data, size_t size, std::unique_ptr<EFPSignalReceiveData>& parsedData);

```
 


## Using EFPSignal in your CMake project

* **Step1** 

Add this in your CMake file.

```
#Include EFPSignal
include(ExternalProject)
ExternalProject_Add(project_efpsignal
        GIT_REPOSITORY https://github.com/agilecontent/efpsignal.git
        GIT_SUBMODULES ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efpsignal
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efpsignal
        GIT_PROGRESS 1
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/efpsignal --config ${CMAKE_BUILD_TYPE} --target efpsignal
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(efpsignal STATIC IMPORTED)
set_property(TARGET efpsignal PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/efpsignal/libefpsignal.a)
add_dependencies(efpsignal project_efpsignal)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/efpsignal")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/efpsignal/efp/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/efpsignal/json/)
```

* **Step2**

Link your library or executable.

```
target_link_libraries((your target) efpsignal (the rest you want to link)) 
```

* **Step3** 

Add header file to your project.

```
#include "efpsignal.h"
```

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Make your additions and write a UnitTest testing it/them.
4. Commit your changes: `git commit -am 'Add some feature'`
5. Push to the branch: `git push origin my-new-feature`
6. Submit a pull request :D

## History

The data content framed using ElasticFrameProtocol is promiscuously added and transported. The receiver is also given what is sent without any declaration of the content. EFPSignal is helping out declaring content and filtering streams of data if configured that way. When using MPEG-TS PAT and PMT has been used to declare content extended by using private data. The older declaration and signaling of media was designed in a simplex world. EFPSignal on the other hand is designed for duplex IP networks for OOB and IB signaling using JSON or binary data types. This enables more flexible ways of working with media flows (pub/sub). For example, a receiver can subscribe to content after receiving the declaration and deciding on what content to subscribe to. It’s also possible to dynamically subscribe and delete content for creating flows adapting to the capacity of the network or the capabilities of the consumer.


## Credits

The UnitX team at Edgeware AB, from 2021 part of Agile Content

## License

*MIT*

Read *LICENSE.md* for details