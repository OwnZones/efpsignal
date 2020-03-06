#include <iostream>
#include "efpsignal.h"

#define MTU 1456 //SRT-max

EFPSignalSend myEFPSignalSend(MTU, 5000);

void declareContent(EFPSignalSend::EFPStreamContent* content) {

}

int main() {
  std::cout << "Hello, World!" << std::endl;

  myEFPSignalSend.declareContentCallback = std::bind(&declareContent, std::placeholders::_1);

  return 0;
}