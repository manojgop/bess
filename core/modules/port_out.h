#ifndef BESS_MODULES_PORTOUT_H_
#define BESS_MODULES_PORTOUT_H_

#include "../module.h"
#include "../module_msg.pb.h"
#include "../port.h"

class PortOut final : public Module {
 public:
  static const gate_idx_t kNumOGates = 0;

  PortOut() : Module(), port_() {}

  pb_error_t Init(const bess::pb::PortOutArg &arg);

  virtual void DeInit() override;

  virtual void ProcessBatch(bess::PacketBatch *batch) override;

  virtual std::string GetDesc() const override;

 private:
  Port *port_;
};

#endif  // BESS_MODULES_PORTOUT_H_
