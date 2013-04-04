#include "impl.h"
#include <utility>
inline int min(int a, int b){
	return a>b?b:a;
}
inline int max(int a, int b){
	return a>b?a:b;
}
//map value of word 
// 00 00 exact match value 0
// 01 01 hamming 1  value 5
// 01 10 hamming 2   value 6
// 01 11 hamming 3   value 7
// 10 01 edit distance 1   value 9
// 10 10 edit distance 2   value 10
// 10 11 edit distance 3   value 11
inline char word_map_value(Query *q){
	return q->match_type*4+q->match_dist;
}
//word a and b are said to be inclusive 
//not found 0 does not help, check not found for greater value
// but if not found is 3 lower would help
inline int notfoundcheck(int map_val, int q_val){
	if (q_val==map_val) return true;
	if (map_val> q_val){
		if ((q_val&0xC)==(map_val&0xc)) //same class
			if ((map_val&0x3) > (q_val&0x3)) return true;
		if ((q_val&0x3)==(map_val&0x3)) //same distance
			if ((map_val&0xc) > (q_val&0xc)) return true;
	}
	return false;
}

//if word with value 1 is in found word with value greater is found too 
inline int foundcheck(int map_val, Query *q){
	int q_val=word_map_value(q);
	if (map_val>=q_val) return false;
	return true;  
}

///////////////////////////////////////////////////////////////////////////////////////////////
int EditDistance(const char* a, int na, const char* b, int nb, int limit)
{
	int oo=10;
	int diff=nb-na;
	if (diff<0) diff=-diff;
	if(diff>limit ) return limit+1;

	static int T[2][MAX_WORD_LENGTH+1];

	int ia, ib;

	int cur=0;
	ia=0;

	for(ib=0;ib<=nb;ib++)
		T[cur][ib]=ib;

	cur=1-cur;

	int stop_b=limit;
	for(ia=1;ia<=na;ia++)
	{
		for(ib=0;ib<=nb;ib++)
			T[cur][ib]=oo;
		int ib_st=0;
		stop_b++;
		int ib_en=min(stop_b, nb);

		if(ib_st==0)
		{
			ib=0;
			T[cur][ib]=ia;
			ib_st++;
		}
		int min=oo;
		for(ib=ib_st;ib<=ib_en;ib++)
		{
			int ret=oo;

			int d1=T[1-cur][ib]+1;
			int d2=T[cur][ib-1]+1;
			int d3=T[1-cur][ib-1]; 
			if(a[ia-1]!=b[ib-1]) d3++;

			if(d1<ret) ret=d1;
			if(d2<ret) ret=d2;
			if(d3<ret) ret=d3;
			if(ret<min) min=ret;
			T[cur][ib]=ret;
		}
		if(min>limit) return limit+1;
		cur=1-cur;
	}

	int ret=T[1-cur][nb];

	return ret;
}


///////////////////////////////////////////////////////////////////////////////////////////////

// Keeps all currently active queries
vector<Query> queries;

// Keeps all currently available results that has not been returned yet
vector<Document> docs;

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode InitializeIndex(){return EC_SUCCESS;}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode DestroyIndex(){return EC_SUCCESS;}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode StartQuery(QueryID query_id, const char* query_str, MatchType match_type, unsigned int match_dist)
{
	Query query;
	query.query_id=query_id;
	strcpy(query.str, query_str);
	query.match_type=match_type;
	query.match_dist=match_dist;
	// Add this query to the active query set
	queries.push_back(query);
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode EndQuery(QueryID query_id)
{
	// Remove this query from the active query set
	unsigned int i, n=queries.size();
	for(i=0;i<n;i++)
	{
		if(queries[i].query_id==query_id)
		{
			queries.erase(queries.begin()+i);
			break;
		}
	}
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////
inline int cmp(const char *a, const char *b, int l, int dist){
	int num_mismatches=0;
	int fail=false;
	for(int i=0;i<l;i++) {
		if(a[i]!=b[i]) {
			num_mismatches++; 
			if (dist+1==num_mismatches){ fail=true; break;}
		}
	}
	return !fail;
}

void GetWords(const char *doc_str,word_list *words){
	int id=0;
	char word[MAX_WORD_LENGTH+1];
	while(doc_str[id]) {
		while(doc_str[id]==' ') id++;
		if(!doc_str[id]) break;
		int ld=id;
		while(doc_str[id] && doc_str[id]!=' '){word[id-ld]=doc_str[id]; id++;}
		word[id-ld]=0;
		ld=id-ld;
		words[ld-MIN_WORD_LENGTH].insert(word);
	}
}

int CheckQuery(Query *quer, word_map &not_found_words){
	int iq=0;
	int matching_query=true;
	while(quer->str[iq] && matching_query)
	{
		while(quer->str[iq]==' ') iq++;
		if(!quer->str[iq]) break;
		char* qword=&quer->str[iq];

		int lq=iq;
		while(quer->str[iq] && quer->str[iq]!=' ') iq++;
		char qt=quer->str[iq];
		quer->str[iq]=0;
		lq=iq-lq;
		word_map::iterator it=not_found_words.find(qword);
		if(it!=not_found_words.end()) {
			// a candidate is found
			int q_val= word_map_value(quer);
			if (q_val>0) q_val= !notfoundcheck(it->second,q_val);
			if(q_val ==0 ){ //>= is better but need to invistage
				//if (it->second!=word_map_value(quer) )printf("%s %d %d\n", it->first.c_str(),it->second,word_map_value(quer));
				matching_query=false;
				quer->str[iq]=qt;
				break;
			}
		}
		quer->str[iq]=qt;
	}
	return matching_query;
}

void AddtoFoundWord(word_map &word_list,const char *word, Query *q){
	char q_val=word_map_value(q);
	word_map::iterator it=word_list.find(word);
	if(it==word_list.end())
		word_list.insert(make_pair<string,char>(word,q_val));
	else 
		if(it->second>q_val)
			it->second= q_val;

}

ErrorCode MatchDocument(DocID doc_id, const char* doc_str)
{
	unsigned int i, n=queries.size();
	vector<unsigned int> query_ids;

	word_list words[MAX_WORD_LENGTH-MIN_WORD_LENGTH];
	word_map not_found_words;
	word_map found_words;

	GetWords(doc_str, words);
	// Iterate on all active queries to compare them with this new document
	for(i=0;i<n;i++)
	{
		bool matching_query=true;
		Query* quer=&queries[i];
		//fail quickly if any word of query is in not_found
		matching_query=CheckQuery(quer,not_found_words);
		if(!matching_query) continue;

		int iq=0;
		while(quer->str[iq] && matching_query)
		{
			while(quer->str[iq]==' ') iq++;
			if(!quer->str[iq]) break;
			char* qword=&quer->str[iq];

			int lq=iq;
			while(quer->str[iq] && quer->str[iq]!=' ') iq++;
			char qt=quer->str[iq];
			quer->str[iq]=0;
			lq=iq-lq;

			bool matching_word=false;
			//first try exact match; even if it is not the case
			word_map::iterator mit;

			if((mit=found_words.find(qword))!=found_words.cend()) {
				int q_val = word_map_value(quer);
				if ( mit->second == q_val || mit->second==0){
					matching_word=true;
				}  else if ( mit->second < q_val){
					int mit_s=mit->second;
					if ( ((q_val&0xC)==(mit_s&0xc)) && ((mit_s&0x3) > (q_val&0x3))) //same class
						matching_word=true;
					else if ( ((q_val&0x3)==(mit_s&0x3)) && ((mit_s&0xc) > (q_val&0xc))) 
						matching_word=true;
				}

			}

			if(!matching_word && words[lq-MIN_WORD_LENGTH].find(qword)!=words[lq-MIN_WORD_LENGTH].end()) {
				matching_word=true;
				AddtoFoundWord(found_words, qword, quer);
			}
			if(!matching_word && quer->match_type!=MT_EXACT_MATCH) {
				for(unordered_set<string>::const_iterator it=words[lq-MIN_WORD_LENGTH].cbegin(); it!=words[lq-MIN_WORD_LENGTH].cend();it++){
					if(cmp(it->c_str(),qword,lq,quer->match_dist)) {
						matching_word=true;
						AddtoFoundWord(found_words,qword/* it->c_str()*/, quer);
						break;
					}
				}
			}  
			if  (quer->match_type==MT_EDIT_DIST && !matching_word) {
				int s_length=lq-quer->match_dist;
				s_length=(s_length<MIN_WORD_LENGTH)?MIN_WORD_LENGTH:s_length;
				int e_length=lq+quer->match_dist;
				e_length=(e_length>MAX_WORD_LENGTH)?MAX_WORD_LENGTH:e_length;

				for(int len=s_length; len <=e_length && !matching_word;len++){
					for(unordered_set<string>::const_iterator it=words[len-MIN_WORD_LENGTH].cbegin(); it!=words[len-MIN_WORD_LENGTH].cend();it++){
						unsigned int edit_dist=EditDistance(qword, lq, it->c_str(), it->length(),quer->match_dist);
						if(edit_dist<=quer->match_dist) {
							matching_word=true; 
							AddtoFoundWord(found_words,qword /*it->c_str()*/, quer);
							break;
						}			

					}
				}
			}
			if(!matching_word)	{
				// This query has a word that does not match any word in the document
				matching_query=false;
				char q_val=word_map_value(quer);
				word_map::iterator it=not_found_words.find(qword);
				if(it==not_found_words.end())
					not_found_words.insert(pair <string, char> (qword,q_val));
				else 
					it->second=max(it->second, q_val);

			}
			quer->str[iq]=qt;
		}/*while(quer->str[iq] && matching_query)*/

		if(matching_query)	{
			// This query matches the document
			query_ids.push_back(quer->query_id);
		}
	}

	Document doc;
	doc.doc_id=doc_id;
	doc.num_res=query_ids.size();
	doc.query_ids=0;
	if(doc.num_res) doc.query_ids=(unsigned int*)malloc(doc.num_res*sizeof(unsigned int));
	for(i=0;i<doc.num_res;i++) doc.query_ids[i]=query_ids[i];
	// Add this result to the set of undelivered results
	docs.push_back(doc);

	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode GetNextAvailRes(DocID* p_doc_id, unsigned int* p_num_res, QueryID** p_query_ids)
{
	// Get the first undeliverd resuilt from "docs" and return it
	*p_doc_id=0; *p_num_res=0; *p_query_ids=0;
	if(docs.size()==0) return EC_NO_AVAIL_RES;
	*p_doc_id=docs[0].doc_id; *p_num_res=docs[0].num_res; *p_query_ids=docs[0].query_ids;
	docs.erase(docs.begin());
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////
