/*
 * Copyright 2014 Andrew Gregory <andrew.gregory.8@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Project URL: http://github.com/andrewgregory/mHashTable.c
 */

#ifndef MHASHTABLE_C
#define MHASHTABLE_C

#include <string.h>
#include <stdlib.h>

#include "mhashtable.h"

static mht_item_t *_mht_lookup(mht_t *mht, const mht_key_t key, size_t hash) {
    mht_item_t *item = mht->buckets[hash % mht->size];
    while(item) {
        if(hash == item->hash && mht->cmpfn(item->key, key) == 0) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

size_t mht_hashfn_sdbm(const char *key) {
    size_t hash = 0;
    while(*key) { hash = *(key++) + (hash << 6) + (hash << 16) - hash; }
    return hash;
}

mht_t *mht_new(size_t buckets) {
    mht_t *h = calloc(1, sizeof(mht_t));
    if(!h) { return NULL; }
    h->size = buckets > 0 ? buckets : MHT_DEFAULT_SIZE;
    h->cmpfn = MHT_DEFAULT_CMPFN;
    h->hashfn = MHT_DEFAULT_HASHFN;
    h->buckets = calloc(h->size, sizeof(mht_item_t*));
    if(!h->buckets) { free(h); return NULL; }
    return h;
}

mht_item_t *mht_next_item(mht_t *table, mht_item_t *item) {
    if(item && item->next) {
        return item->next;
    } else {
        size_t i = item ? item->hash % table->size + 1 : 0;
        while(i < table->size) {
            if(table->buckets[i]) { return table->buckets[i]; }
            i++;
        }
    }
    return NULL;
}

void mht_free(mht_t *mht) {
    if(mht) {
        mht_item_t *i, *next = mht_next_item(mht, NULL);
        while((i = next)) {
            next = mht_next_item(mht, i);
            free(i);
        }
        free(mht->buckets);
        free(mht);
    }
}

mht_item_t *mht_get_item(mht_t *mht, const mht_key_t key) {
    return _mht_lookup(mht, key, mht->hashfn(key));
}

int mht_delete_item(mht_t *mht, const mht_key_t key) {
    mht_item_t *i, *item = mht_get_item(mht, key);
    size_t index;
    if(item == NULL) { return 0; }
    index = item->hash % mht->size;
    i = mht->buckets[index];
    if(i == item) {
        mht->buckets[index] = item->next;
        free(item);
    } else {
        while(i->next && i->next != item) { i = i->next; }
        i->next = item->next;
        free(item);
    }
    return 1;
}

int mht_set_value(mht_t *mht, const mht_key_t key, const mht_value_t value) {
    size_t hash = mht->hashfn(key);
    mht_item_t *item = _mht_lookup(mht, key, hash);
    if(item) {
        item->value = value;
    } else {
        size_t index = hash % mht->size;
        if((item = calloc(1, sizeof(mht_item_t))) == NULL) { return 0; }
        item->hash = hash;
        item->key = key;
        item->value = value;
        item->next = mht->buckets[index];
        mht->buckets[index] = item;
    }
    return 1;
}

mht_value_t mht_get_value(mht_t *mht, const mht_key_t key) {
    mht_item_t *i = mht_get_item(mht, key);
    return i ? (void*) i->value : NULL;
}

int mht_contains(mht_t *mht, const mht_key_t key) {
    return mht_get_item(mht, key) ? 1 : 0;
}

#endif /* MHASHTABLE_C */
