#pragma once

#include <stdint.h>
#include "tx_certificate_types.h"  // For ext_voter_t, vote_t, gov_action_id_t, anchor_t
#include "memory/flist.h"

// A single vote: gov_action_id + voting_procedure
typedef struct {
    gov_action_id_t govActionId;      // Tx hash (pointer) + index
    vote_t voteOption;                 // NO=0, YES=1, ABSTAIN=2
    anchor_t anchor;                   // Optional anchor (URL + hash)
} vote_item_t;

// List node for individual votes (inner map entries)
typedef struct {
    s_flist_node node;
    vote_item_t vote_data;
} vote_list_item_t;

// A voter with their votes
typedef struct {
    ext_voter_t voter;                 // Voter (key_path, key_hash, or script_hash)
    uint16_t numVotes;                 // Number of votes for this voter
    s_flist_node* votes;               // Linked list of vote_list_item_t
} voter_votes_t;

// List node for voters (outer map entries)
typedef struct {
    s_flist_node node;
    voter_votes_t voter_votes_data;
} voter_votes_list_item_t;
