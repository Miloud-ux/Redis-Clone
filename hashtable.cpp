#include <cassert>
#include <cstdlib>
#include <functional>
#include <vector>

#include "hashtable.h"
#include "server.h"

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128; // migrate 128 entries per rehash

static struct {
  HMap db;
} g_data;

static void h_init(HTab *t, size_t n) {
  assert(n > 0 && ((n - 1) & n) == 0 && t != NULL);
  t->tab = (HNode **)calloc(n, sizeof(HNode *));
  t->size = 0;
  t->mask = n - 1;
}

static bool entry_eq(HNode *n1, HNode *n2) {
  Entry *en1 = container_of(n1, struct Entry, node);
  Entry *en2 = container_of(n2, struct Entry, node);
  return en1->key == en2->key;
}

static bool key_eq(HNode *n1, HNode *n2) {
  LookupKey *k1 = container_of(n1, LookupKey, node);
  LookupKey *k2 = container_of(n2, LookupKey, node);
  return k1 == k2;
}

// We don't allocate in the data structure code since it's intrusive
static void h_insert(HTab *t, HNode *n) {
  size_t pos = n->hcode & t->mask;
  n->next = t->tab[pos];
  t->tab[pos] = n;
  t->size++;
  // TODO: log insertion or something
}

static HNode **h_lookup(HTab *t, HNode *key, bool (*eq)(HNode *, HNode *)) {
  if (!t->tab) {
    return NULL;
  }

  size_t pos = key->hcode & t->mask;
  HNode **from = &t->tab[pos];

  for (HNode *curr; (curr = *from) != NULL; from = &curr->next) {
    if (curr->hcode == key->hcode && eq(key, curr)) {
      return from; // return parent pointer for deletion
    }
  }

  return NULL;
}

static HNode *h_detach(HTab *t, HNode **from) {
  HNode *node = *from;
  *from = node->next;
  t->size--;
  return node;
}

static void hm_trigger_rehashing(HMap *m) {
  m->older = m->newer;
  h_init(&m->newer, (m->older.mask + 1) * 2);
  m->migrate_pos = 0;
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  HNode **from = h_lookup(&hmap->newer, key, eq);
  if (!from) {
    from = h_lookup(&hmap->older, key, eq);
  }

  return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
    return h_detach(&hmap->newer, from);
  }
  if (HNode **from = h_lookup(&hmap->older, key, eq)) {
    return h_detach(&hmap->older, from);
  }
  return NULL;
}

void hm_insert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    h_init(&hmap->newer, 4);
  }

  h_insert(&hmap->newer, node);

  if (!hmap->older.tab) // if we are not during a rehash
  {
    size_t threshhold = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= threshhold) {
      hm_trigger_rehashing(hmap);
    }
  }

  hm_help_rehashing(hmap); // migrate some keys
}

void hm_help_rehashing(HMap *hmap) {
  size_t nwork = 0;

  while (nwork < k_rehashing_work && hmap->older.size > 0) {
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }

    h_insert(&hmap->newer, h_detach(&hmap->older, from));
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

// [hash(age)] -> 20
// get age

/* create a dummy entry
 * dummy->node = NULL; dummy->key = cmd[2]; dummy->val = 0;
 * dummyHnode = {}; dummyHnode->key = hash(cmd[2])
 * target = lookup(hmap, dummy);
 * if(target != NULL) we found it so we return val
 * else return error
 */

static void do_get(std::vector<std::string> &cmd, Response &out) {
  LookupKey dummy = {};
  dummy.key.swap(cmd[2]);

  dummy.node.hcode = str_hash((uint8_t *)dummy.key.data(), dummy.key.size());

  HNode *target = hm_lookup(&g_data.db, &dummy.node, &key_eq);
  if (!target) {
    out.status = RES_NX;
    return;
  }

  const std::string &val = container_of(target, struct Entry, node)->value;
  assert(val.size() < k_max_msg);
  out.data.assign(val.begin(), val.end());
}
