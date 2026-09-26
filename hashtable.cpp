#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <openssl/sha.h>

#include "hashtable.h"
#include "server.h"

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128;

static struct {
  HMap db;
} g_data;

static void h_init(HTab *t, size_t n) {
  assert(n > 0 && ((n - 1) & n) == 0 && t != NULL);
  t->tab = (HNode **)calloc(n, sizeof(HNode *));
  t->size = 0;
  t->mask = n - 1;
}

static bool entry_eq(HNode *lhs, HNode *rhs) {
  Entry *le = container_of(lhs, Entry, node);
  Entry *re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

static void h_insert(HTab *t, HNode *n) {
  size_t pos = n->hcode & t->mask;
  n->next = t->tab[pos];
  t->tab[pos] = n;
  t->size++;
}

static HNode **h_lookup(HTab *t, HNode *key, bool (*eq)(HNode *, HNode *)) {
  if (!t->tab) {
    return NULL;
  }

  size_t pos = key->hcode & t->mask;
  HNode **from = &t->tab[pos];
  for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
    if (cur->hcode == key->hcode && eq(key, cur)) {
      return from;
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

static void hm_trigger_rehashing(HMap *hmap) {
  hmap->older = hmap->newer;
  h_init(&hmap->newer, (hmap->older.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

static size_t hm_size(HMap *hmap) { return hmap->newer.size + hmap->older.size; }

static void hm_foreach(HMap *hmap, bool (*cb)(HNode *, void *), void *arg) {
  if (hmap->newer.tab) {
    for (size_t i = 0; i <= hmap->newer.mask; i++) {
      for (HNode *node = hmap->newer.tab[i]; node; node = node->next) {
        if (!cb(node, arg)) {
          return;
        }
      }
    }
  }
  if (hmap->older.tab) {
    for (size_t i = 0; i <= hmap->older.mask; i++) {
      for (HNode *node = hmap->older.tab[i]; node; node = node->next) {
        if (!cb(node, arg)) {
          return;
        }
      }
    }
  }
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

  if (!hmap->older.tab) {
    size_t threshold = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= threshold) {
      hm_trigger_rehashing(hmap);
    }
  }
  hm_help_rehashing(hmap);
}

void hm_help_rehashing(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < k_rehashing_work && hmap->older.size > 0) {
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }
    HNode *node = h_detach(&hmap->older, from);
    h_insert(&hmap->newer, node);
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

uint64_t str_hash(const uint8_t *data, size_t len) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(data, len, digest);
  uint64_t res = 0;
  for (size_t i = 0; i < 8; i++) {
    res = (res << 8) | digest[i];
  }
  return res;
}

static void do_get(std::vector<std::string> &cmd, Buffer &out) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((const uint8_t *)key.key.data(), key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if (!node) {
    return out_nil(out);
  }

  const std::string &val = container_of(node, Entry, node)->value;
  assert(val.size() <= k_max_msg);
  return out_str(out, val.data(), val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((const uint8_t *)key.key.data(), key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if (node) {
    container_of(node, Entry, node)->value.swap(cmd[2]);
  } else {
    Entry *entry = new Entry();
    entry->key.swap(key.key);
    entry->value.swap(cmd[2]);
    entry->node.hcode = str_hash((const uint8_t *)entry->key.data(), entry->key.size());
    hm_insert(&g_data.db, &entry->node);
  }
  return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((const uint8_t *)key.key.data(), key.key.size());

  HNode *node = hm_delete(&g_data.db, &key.node, &entry_eq);
  if (node) {
    delete container_of(node, Entry, node);
  }
  return out_int(out, node ? 1 : 0);
}

static bool cb_keys(HNode *node, void *arg) {
  Buffer &out = *(Buffer *)arg;
  const std::string &key = container_of(node, Entry, node)->key;
  out_str(out, key.data(), key.size());
  return true;
}

static void do_keys(std::vector<std::string> &, Buffer &out) {
  out_arr(out, (uint32_t)hm_size(&g_data.db));
  hm_foreach(&g_data.db, cb_keys, &out);
}

static void do_dbsize(std::vector<std::string> &, Buffer &out) {
  out_int(out, (int64_t)hm_size(&g_data.db));
}

void do_request(std::vector<std::string> &cmd, Buffer &out) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    do_get(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    do_set(cmd, out);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    do_del(cmd, out);
  } else if (cmd.size() == 1 && cmd[0] == "keys") {
    do_keys(cmd, out);
  } else if (cmd.size() == 1 && cmd[0] == "dbsize") {
    do_dbsize(cmd, out);
  } else {
    out_err(out, ERR_UNKNOWN, "unknown cmd", sizeof("unknown cmd") - 1);
  }
}
