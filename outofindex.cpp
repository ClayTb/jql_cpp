// out_of_range example
#include <iostream>       // std::cerr
#include <stdexcept>      // std::out_of_range
#include <vector>         // std::vector
#include <unistd.h>
#include "loguru.cpp"


int main(int argc, char* argv[]) {
    	loguru::init(argc, argv);

  std::vector<int> myvector(10);
  try {
    myvector.at(20)=100;      // vector::at throws an out-of-range
  }
  catch (const std::out_of_range& oor) {
    std::cerr << "Out of Range error: " << oor.what() << '\n';
  }
  while(true)
  {
      std::cout<<"in loop\n";
      sleep(1);
  }
  return 0;
}