#include <cstring>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <pthread.h>
#include <semaphore.h>
#include "pth.h"
#include <utility>
#include <bitset>
#include "core.h"

using namespace std;

typedef unordered_set<string> word_list;
typedef unordered_map<string,char> word_map;

unordered_map<DocID, char *> docs_str;
// Keeps all information related to an active query
struct Query
{
    QueryID query_id;
    char str[MAX_QUERY_LENGTH];
    MatchType match_type;
    unsigned int match_dist;
};
sem_t docMutex;
sem_t docsMutex;
sem_t mqMutex;
unordered_map<DocID, vector<Query> > m_queries;

thpool_t* pool;
int isInit=0;

typedef struct doc_thread_prams
{
    DocID doc_id;
}doc_thread_prams;

///////////////////////////////////////////////////////////////////////////////////////////////

// Keeps all query ID results associated with a dcoument
struct Document
{
    DocID doc_id;
    unsigned int num_res;
    QueryID* query_ids;
};

inline int min(int a, int b){
return a>b?b:a;
}
inline int max(int a, int b){
return a>b?a:b;
}
inline char word_map_value(Query *q){
    return q->match_type*4+q->match_dist;
}
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


///////////////////////////////////////////////////////////////////////////////////////////////
unsigned int inline min3 (unsigned int a, unsigned int b, unsigned int c)
{
    __asm__ (
    "cmp     %0, %1\n\t"
    "cmovle  %1, %0\n\t"
    "cmp     %0, %2\n\t"
    "cmovle  %2, %0\n\t"
    : "+r"(a) :
        "%r"(b), "r"(c)
      );
    return a;
}

unsigned int EditDistance(const char *s1,unsigned int s1len,const char *s2,unsigned int s2len, unsigned int limit) {
    unsigned int  x, y, lastdiag, olddiag;
    unsigned int column[s1len+1];
    for (y = 1; y <= s1len; y++)
        column[y] = y;
    for (x = 1; x <= s2len; x++) {
        column[0] = x;
        unsigned int mini=0x7FFFFFFF;
        for (y = 1, lastdiag = x-1; y <= s1len; y++) {
            olddiag = column[y];
            column[y] = min3(column[y] + 1, column[y-1] + 1, lastdiag + (s1[y-1] == s2[x-1] ? 0 : 1));
            lastdiag = olddiag;
            mini=min(mini,column[y]);
        }
        if (mini > limit)
            return limit+1;
    }
    return(column[s1len]);
}



///////////////////////////////////////////////////////////////////////////////////////////////

// Keeps all currently active queries
vector<Query> queries;

// Keeps all currently available results that has not been returned yet
deque<Document> docs;

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode InitializeIndex(){
 
/*	pthread_mutex_init(&ndocsMutex,NULL);
	pthread_cond_init (&notFull, NULL);
	pthread_cond_init (&notEmpty, NULL);*/
return EC_SUCCESS;}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode DestroyIndex(){return EC_SUCCESS;}

///////////////////////////////////////////////////////////////////////////////////////////////

void pauseQuery() {

}
ErrorCode StartQuery(QueryID query_id, const char* query_str, MatchType match_type, unsigned int match_dist)
{
    pauseQuery();
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
    pauseQuery();
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

void GetWords(const char *doc_str,word_list *words, bitset<32> &lengths){
    int id=0;
    char word[MAX_WORD_LENGTH+1];
    while(doc_str[id]) {
        while(doc_str[id]==' ') id++;
        if(!doc_str[id]) break;
        int ld=id;

        while(doc_str[id] && doc_str[id]!=' '){word[id-ld]=doc_str[id]; id++;}
        word[id-ld]=0;
        ld=id-ld;
	// if this length is not needed, do not even bother to store it
	if (lengths[ld-MIN_WORD_LENGTH])
	        words[ld-MIN_WORD_LENGTH].insert(word);
	
    }
}
//never change query str in place, it may be used by another thread
int CheckQuery(Query *quer, word_map &not_found_words, bitset<32> &lengths){
    int iq=0;
    int matching_query=true;
    while(quer->str[iq] && matching_query)
    {
	while(quer->str[iq]==' ') iq++;
        if(!quer->str[iq]) break;
        char qword[MAX_WORD_LENGTH+1];

        int lq=iq;
        while(quer->str[iq] && quer->str[iq]!=' '){qword[iq-lq]=quer->str[iq]; iq++;}
        qword[iq-lq]=0;
        lq=iq-lq;

        word_map::iterator it=not_found_words.find(qword);
        if(it!=not_found_words.end()) {
            // a candidate is found
		int q_val= word_map_value(quer);
		if (q_val>0) q_val= !notfoundcheck(it->second,q_val);
		if(q_val ==0 ){
			matching_query=false;
			break;
		}
        }
    }
    return matching_query;
}
void QueryLength(Query *quer, bitset<32>  &lengths){
	int iq=0;
	while(quer->str[iq]) {
		while(quer->str[iq]==' ') iq++;
		if(!quer->str[iq]) break;
		
		int lq=iq;
		while(quer->str[iq] && quer->str[iq]!=' ') iq++;
		lq=iq-lq;
		
		if (quer->match_type==MT_EDIT_DIST) {
			for(int i=max(lq-quer->match_dist,MIN_WORD_LENGTH);i<=min(lq+quer->match_dist,MAX_WORD_LENGTH);i++)
				lengths.set(i-MIN_WORD_LENGTH);
		} else 
			lengths.set(lq-MIN_WORD_LENGTH);

	}
}

///////////////////////////////////////////////////////////

void AddtoFoundWord(word_map &word_list,const char *word, Query *q){
	char q_val=word_map_value(q);
	word_map::iterator it=word_list.find(word);
	if(it==word_list.end())
		word_list.insert(pair<string,char>(word,q_val));
	else 
		if(it->second>q_val)
			it->second= q_val;

}
//get query lengths

void* DocumentThread(void* data_in)
{
    DocID doc_id = ((doc_thread_prams *)data_in)->doc_id;
    vector<Query> *queriesCopy=NULL;
    sem_wait(&mqMutex);
    queriesCopy = &(m_queries[(doc_id-1) /48]);
    sem_post(&mqMutex);
    
    char *doc_str;    
    sem_wait(&docsMutex);
    doc_str=docs_str[doc_id];
    sem_post(&docsMutex);


    unsigned int i, n=queriesCopy->size();
    vector<unsigned int> query_ids;
    word_list words[MAX_WORD_LENGTH-MIN_WORD_LENGTH];
    word_map not_found_words;
    word_map found_words;
    bitset<32> lengths;

   for(i=0;i<n;i++) {
	Query* quer=&(queriesCopy->operator[](i));
	QueryLength(quer,lengths);
    }
    GetWords(doc_str, words, lengths);

    // Iterate on all active queries to compare them with this new document
    for(i=0;i<n;i++)
    {
        bool matching_query=true;
        Query* quer=&(queriesCopy->operator[](i));
        matching_query=CheckQuery(quer,not_found_words, lengths);
        if(!matching_query) continue;

        int iq=0;
        while(quer->str[iq] && matching_query)
        {
            while(quer->str[iq]==' ') iq++;
            if(!quer->str[iq]) break;
	    char qword[MAX_WORD_LENGTH+1];

	    int lq=iq;
            while(quer->str[iq] && quer->str[iq]!=' '){qword[iq-lq]=quer->str[iq]; iq++;}
            qword[iq-lq]=0;
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
  				AddtoFoundWord(found_words,qword, quer);
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
                        not_found_words.insert(make_pair<string,char> (string(qword),char(q_val)));
                    else
                        it->second=max(it->second, q_val);
            }

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
    sem_wait(&docMutex);
    docs.push_back(doc);
    sem_post(&docMutex);


    doc_thread_prams* temp = (doc_thread_prams*)data_in;
    delete [] doc_str;
    sem_wait(&docsMutex);
    docs_str.erase(doc_id);
    sem_post(&docsMutex);

    delete temp;

}

ErrorCode MatchDocument(DocID doc_id, const char* doc_str)
{
if (!isInit){
	pool = thpool_init(24);
        isInit = 1;
        sem_init(&docMutex,0,1);
	sem_init(&docsMutex,0,1);
	sem_init (&mqMutex,0,1);
}

     int g_id=(doc_id-1)/48;
     int g_id_old=(doc_id-2)/48;
     if(g_id!=g_id_old){
      vector<Query> c_q;
	for(auto it=queries.begin(); it!=queries.end();it++) {
		Query q;
		q.query_id=it->query_id;
		strcpy(q.str, it->str);
		q.match_type=it->match_type;
		q.match_dist=it->match_dist;
		c_q.push_back(q);
	}
    sem_wait(&mqMutex);
       m_queries[g_id]=c_q;
    sem_post(&mqMutex);


}

    doc_thread_prams* thread_data= new doc_thread_prams;
    thread_data->doc_id = doc_id;
    
     long doc_len=strlen(doc_str);
     char* tmp=new char[doc_len+1];
     strcpy(tmp, doc_str);
    sem_wait(&docsMutex);
     docs_str[doc_id]=tmp;
    sem_post(&docsMutex);

     thpool_add_work(pool,
	DocumentThread,(void*) thread_data);


    return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode GetNextAvailRes(DocID* p_doc_id, unsigned int* p_num_res, QueryID** p_query_ids)
{
    while(1)
    {
        sem_wait(&docMutex);
        if(docs.size()>0)
            break;
        sem_post(&docMutex);
    }
    sem_post(&docMutex);
    *p_doc_id=docs[0].doc_id;
    *p_num_res=docs[0].num_res;
    *p_query_ids=docs[0].query_ids;

    docs.pop_front();
    return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////
