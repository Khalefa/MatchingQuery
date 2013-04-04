#include "grams.h"
unordered_map<int,Grams> grams;
map<int, Query > queries_map;

unordered_map<string, int> query_words;

void insert_word(char * s, int qid);

void grams_insert_word(char *word, int lw){
int hash_val=0;

}

void build_grams(){
char word[31];
//iterate on query words
for(i=0;i<n;i++) {
		
	Query* quer=&queries[i];
	if (quer->match_type!=MT_EDIT_DIST) continue;
	int iq=0;
	while(quer->str[iq]){
		while(quer->str[iq]==' ') iq++;
		if(!quer->str[iq]) break;
		char* qword=&quer->str[iq];
		int lq=iq;
		while(quer->str[iq] && quer->str[iq]!=' ') {word[iq-s_iq]=quer->str[iq]; iq++;}
		char qt=quer->str[iq];
		quer->str[iq]=0;
		lq=iq-lq;
		
		grams_insert_word(qword,lq);
		quer->str[iq]=qt;
				
	}			


}



