/*******************************************************************************
 *   Ledger Cardano App
 *   (c) 2016-2025 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#include "flist.h"

void flist_push_front(s_flist_node **list, s_flist_node *node) {
    if ((list != NULL) && (node != NULL)) {
        node->next = *list;
        *list = node;
    }
}

void flist_pop_front(s_flist_node **list, f_list_node_del func) {
    s_flist_node *tmp;

    if (list != NULL) {
        tmp = *list;
        if (tmp != NULL) {
            *list = tmp->next;
            if (func != NULL) func(tmp);
        }
    }
}

void flist_push_back(s_flist_node **list, s_flist_node *node) {
    s_flist_node *tmp;

    if ((list != NULL) && (node != NULL)) {
        if (*list == NULL) {
            *list = node;
        } else {
            tmp = *list;
            if (tmp != NULL) {
                for (; tmp->next != NULL; tmp = tmp->next)
                    ;
                tmp->next = node;
            }
        }
    }
}

void flist_pop_back(s_flist_node **list, f_list_node_del func) {
    s_flist_node *tmp;

    if (list != NULL) {
        tmp = *list;
        if (tmp != NULL) {
            // only one element
            if (tmp->next == NULL) {
                flist_pop_front(list, func);
            } else {
                for (; tmp->next->next != NULL; tmp = tmp->next)
                    ;
                if (func != NULL) func(tmp->next);
                tmp->next = NULL;
            }
        }
    }
}

void flist_insert_after(s_flist_node **list, s_flist_node *ref, s_flist_node *node) {
    (void) list;
    if ((ref != NULL) && (node != NULL)) {
        node->next = ref->next;
        ref->next = node;
    }
}

void flist_remove(s_flist_node **list, s_flist_node *node, f_list_node_del func) {
    s_flist_node *it;
    s_flist_node *tmp;

    if ((list != NULL) && (node != NULL)) {
        if (node == *list) {
            // first element
            flist_pop_front(list, func);
        } else {
            it = *list;
            if (it != NULL) {
                for (; (it->next != node) && (it->next != NULL); it = it->next)
                    ;
                if (it->next == NULL) {
                    // node not found
                    return;
                }
                tmp = it->next->next;
                if (func != NULL) func(it->next);
                it->next = tmp;
            }
        }
    }
}

void flist_clear(s_flist_node **list, f_list_node_del func) {
    s_flist_node *tmp;
    s_flist_node *next;

    if (list != NULL) {
        tmp = *list;
        while (tmp != NULL) {
            next = tmp->next;
            if (func != NULL) func(tmp);
            tmp = next;
        }
        *list = NULL;
    }
}

size_t flist_size(s_flist_node *const *list) {
    size_t size = 0;

    if (list != NULL) {
        for (s_flist_node *tmp = *list; tmp != NULL; tmp = tmp->next) size += 1;
    }
    return size;
}

void flist_sort(s_flist_node **list, f_list_node_cmp func) {
    s_flist_node **tmp;
    s_flist_node *a, *b;
    bool sorted;

    if ((list != NULL) && (func != NULL)) {
        do {
            sorted = true;
            for (tmp = list; (*tmp != NULL) && ((*tmp)->next != NULL); tmp = &(*tmp)->next) {
                a = *tmp;
                b = a->next;
                if (func(a, b) == false) {
                    *tmp = b;
                    a->next = b->next;
                    b->next = a;
                    sorted = false;
                }
            }
        } while (!sorted);
    }
}
