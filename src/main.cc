#include "afmpb.h"

int main(int argc, char **argv) {
  auto parameter = afmpb::init(argc, argv); 

  if (parameter) {
    afmpb::AFMPB system(std::move(parameter)); 

    auto status = system.computePotential(); 

    system.computeEnergy(status); 

    system.finalize(status); 
  } 

  auto err = afmpb::finalize(); 
  assert(err == 0); 

  return 0;
}

