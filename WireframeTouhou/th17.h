#ifndef TH17_H
#define TH17_H

#include "state_reader.h"

namespace th {

class Th17Reader : public StateReader {
 public:
  int GetId() const override;
  int Check() const override;
  int Read(GameState *state) const override;
  StateReader *Clone() const override;
  int Hook() const override;
  int Unhook() const override;
  DWORD_PTR GetImageBase() const override;

  static const int kGameId = 17;

 public:
  static const GameStateConfig kConfig;
};
}
#endif
