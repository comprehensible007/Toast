#pragma once
#include "mapper.h"
#include <memory>

std::unique_ptr<Mapper> CreateMapper(int mapperID, std::vector<u8> prg, std::vector<u8> chr,
                                      bool chrIsRAM, Mirroring headerMirroring);
