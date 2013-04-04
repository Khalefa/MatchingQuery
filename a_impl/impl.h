#include "core.h"
///////////////////////////////////////////////////////////////////////////////////////////////
#include <cstring>
#include <string>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <vector>
#include <unordered_set>
#include <unordered_map>
using namespace std;

typedef unordered_set<string> word_list;
typedef unordered_map<string,char> word_map;


// Keeps all information related to an active query
struct Query
{
	QueryID query_id;
	char str[MAX_QUERY_LENGTH];
	MatchType match_type;
	unsigned int match_dist;
};

///////////////////////////////////////////////////////////////////////////////////////////////

// Keeps all query ID results associated with a dcoument
struct Document
{
	DocID doc_id;
	unsigned int num_res;
	QueryID* query_ids;
};

