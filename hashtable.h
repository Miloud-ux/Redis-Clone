#pragma once
#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <string>
#include <stdlib.h>

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

struct HNode {
  HNode *next = NULL;
  uint64_t hcode = 0;
};

struct HTab {
  HNode **tab = NULL;
  size_t size = 0;
  size_t mask = 0;
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

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_help_rehashing(HMap *hmap);