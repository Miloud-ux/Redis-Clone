
#include "server.h"
#include <cstdint>
#include <stdlib.h>
#include <string>
#include <vector>

// intrusive data structure
#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

struct HNode {
  HNode *next = NULL;
  uint64_t hcode = 0; // hash value
};

struct HTab {
  HNode **tab = NULL; // array of buckets
  size_t size = 0;    // number of keys
  size_t mask = 0;    // 2^n - 1 where (2^n) is the size
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos;
};

struct Entry {
  HNode node;
  std::string key;
  std::string value;
};

// for lookup only
struct LookupKey {
  HNode node;
  std::string key;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *node, bool (*eq)(HNode *, HNode *));
void hm_help_rehashing(HMap *hmap);

void do_del(std::vector<std::string> &cmd, Buffer &out);
void do_set(std::vector<std::string> &cmd, Buffer &out);
void do_get(std::vector<std::string> &cmd, Buffer &out);
