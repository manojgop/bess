#ifndef BESS_MODULES_WILDCARDMATCH_H_
#define BESS_MODULES_WILDCARDMATCH_H_

#include "../module.h"

#include <rte_config.h>
#include <rte_hash_crc.h>

#include "../module_msg.pb.h"
#include "../utils/htable.h"

#define MAX_TUPLES 8
#define MAX_FIELDS 8
#define MAX_FIELD_SIZE 8
static_assert(MAX_FIELD_SIZE <= sizeof(uint64_t),
              "field cannot be larger than 8 bytes");

#define HASH_KEY_SIZE (MAX_FIELDS * MAX_FIELD_SIZE)

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error this code assumes little endian architecture (x86)
#endif

struct WmData {
  int priority;
  gate_idx_t ogate;
};

struct WmField {
  int attr_id; /* -1 for offset-based fields */

  /* Relative offset in the packet data for offset-based fields.
   *  (starts from data_off, not the beginning of the headroom */
  int offset;

  int pos; /* relative position in the key */

  int size; /* in bytes. 1 <= size <= MAX_FIELD_SIZE */
};

struct wm_hkey_t {
  uint64_t u64_arr[MAX_FIELDS];
};

class WildcardMatch final : public Module {
 public:
  static const gate_idx_t kNumOGates = MAX_GATES;

  static const Commands cmds;

  WildcardMatch()
      : Module(),
        default_gate_(),
        total_key_size_(),
        fields_(),
        tuples_() {}

  pb_error_t Init(const bess::pb::WildcardMatchArg &arg);

  virtual void DeInit() override;

  virtual void ProcessBatch(bess::PacketBatch *batch) override;

  virtual std::string GetDesc() const override;

  pb_cmd_response_t CommandAdd(const bess::pb::WildcardMatchCommandAddArg &arg);
  pb_cmd_response_t CommandDelete(
      const bess::pb::WildcardMatchCommandDeleteArg &arg);
  pb_cmd_response_t CommandClear(const bess::pb::EmptyArg &arg);
  pb_cmd_response_t CommandSetDefaultGate(
      const bess::pb::WildcardMatchCommandSetDefaultGateArg &arg);

 private:
  static int wm_keycmp(const void *key, const void *key_stored,
                       size_t key_len) {
    const uint64_t *a = ((wm_hkey_t *)key)->u64_arr;
    const uint64_t *b = ((wm_hkey_t *)key_stored)->u64_arr;

    switch (key_len >> 3) {
      default:
        promise_unreachable();
      case 8:
        if (unlikely(a[7] != b[7]))
          return 1;
      case 7:
        if (unlikely(a[6] != b[6]))
          return 1;
      case 6:
        if (unlikely(a[5] != b[5]))
          return 1;
      case 5:
        if (unlikely(a[4] != b[4]))
          return 1;
      case 4:
        if (unlikely(a[3] != b[3]))
          return 1;
      case 3:
        if (unlikely(a[2] != b[2]))
          return 1;
      case 2:
        if (unlikely(a[1] != b[1]))
          return 1;
      case 1:
        if (unlikely(a[0] != b[0]))
          return 1;
    }

    return 0;
  }

  static uint32_t wm_hash(const void *key, uint32_t key_len,
                          uint32_t init_val) {
#if __SSE4_2__ && __x86_64
    const uint64_t *a = ((wm_hkey_t *)key)->u64_arr;

    switch (key_len >> 3) {
      default:
        promise_unreachable();
      case 8:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 7:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 6:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 5:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 4:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 3:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 2:
        init_val = crc32c_sse42_u64(*a++, init_val);
      case 1:
        init_val = crc32c_sse42_u64(*a++, init_val);
    }

    return init_val;
#else
    return rte_hash_crc(key, key_len, init_val);
#endif
  }

  struct WmTuple {
    HTable<wm_hkey_t, struct WmData, wm_keycmp, wm_hash> ht;
    wm_hkey_t mask;
  };

  gate_idx_t LookupEntry(wm_hkey_t *key, gate_idx_t def_gate);

  pb_error_t AddFieldOne(const bess::pb::WildcardMatchArg_Field &field,
                         struct WmField *f);

  template <typename T>
  pb_error_t ExtractKeyMask(const T &arg, wm_hkey_t *key, wm_hkey_t *mask);

  int FindTuple(wm_hkey_t *mask);
  int AddTuple(wm_hkey_t *mask);
  int DelEntry(int idx, wm_hkey_t *key);

  gate_idx_t default_gate_;

  int total_key_size_; /* a multiple of sizeof(uint64_t) */

  std::vector<struct WmField> fields_;

  std::vector<struct WmTuple> tuples_;
};

#endif  // BESS_MODULES_WILDCARDMATCH_H_
