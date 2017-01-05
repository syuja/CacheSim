/***********************************************************
* *
* Name: Samir Yuja 
* Class: CDA3101 
* Assignment: Implementing a Data Cache Simulator 
* Compile: "g++47 -std=c++11 -o datacache yuja_datacache.cpp"
*
*Please look at main to see the sequence of function calls!
*Also please note! A dump Function is provided for Grading Purposes!
* *
***********************************************************/


/***********************************************************
				DATA CACHE SIMULATOR
**********************************************************/


#include<iostream>
#include<sstream>
#include<vector>
#include<queue>
#include<cmath>
#include<fstream>
#include<cstdio>
#include<iomanip>
#include<cstdlib>
	using namespace std;
	
//typedef: to reduce length of lines
typedef unsigned int u_int;

//***CONSTANT VARIABLES***//
static const u_int ADDRESS_SIZE = 32;
static const string TRACE_CONFIG = "trace.config";

struct Mem_ref{
	string access_type;
	u_int size, hex_address;	//in trace.config
	u_int tag, index, offset; 	//address_extractor() gets
	
	//constructor for Mem_ref for initializing to 0
	Mem_ref(string access_t, u_int si, u_int hex,
			u_int ta= 0,u_int ind= 0,u_int off= 0) 
		:access_type(access_t), size(si), hex_address(hex),
		 tag(ta),index(ind),offset(off) {};

};


//***GLOBAL VARIABLES (called across functions)***//
u_int cache_size, set_size, line_size;		  
u_int tag_f_size, index_f_size, offset_f_size;//size in bits of fields
u_int total_hits;							  //counter for hits
vector<Mem_ref> refs; 						  //holds valid mem_refs	
vector<string> result;						  //hit or miss at that ref
vector<u_int> mem_accesses;					  //# of memory references
queue<string> errors; 						  //save errors for printResults


struct Block{
	u_int tag;								  //for matching
	u_int lru_counter;						  //for replacing
	bool valid_bit;							  //contents valid?
	bool dirty_bit;							  //for write-back
	
	//block default constructor
	Block( u_int t = 0, u_int lru_c = 0,bool valid_b = false,
		 bool dirty_b = false) :  tag(t), lru_counter(lru_c),
			valid_bit(valid_b), dirty_bit(dirty_b)
		  {};
	
};


struct Cache{
	//simulates the cache! 2d array of blocks!
	struct Block** cache; //more efficient than 8192 x 8 static array
	
	//cache constructor: fills with empty blocks
 	Cache(){
			cache = new Block*[cache_size]; //4
			for(u_int i = 0; i < cache_size; ++i){
			cache[i] = new Block[set_size];}
		};
		
	//copy constructor
	Cache(const struct Cache &x){
		//new cache of same size
		cache = new Block*[cache_size];
		for(u_int i = 0; i < cache_size; ++i){
			cache[i] = new Block[set_size];
			for(u_int j = 0; j < set_size; ++j){
				//copy the individual blocks over
				cache[i][j] = x.cache[i][j];
			}
		}
	}
	
	//cache destructor
	~Cache(){
		for(u_int i = 0; i < cache_size; ++i){
			delete cache[i];
		}
		 delete cache;
	}; 
};



/*
	Steps follow function calls in main!
*/

//step 1
//reads trace.config and initializes sizes 
void read_configuration();
void calc_field_size(); 

//step 2
//read mem_refs, extracts address and save in refs vector
void read_references();
void address_extractor(u_int hex_a, u_int &tag, u_int &index, u_int &offset);

//step 3
//run the simulator using LRU replacement policy
void Simulator(const struct Cache &C);
void LRU_reset(const struct Cache &C,u_int set_ind, u_int block);
u_int LRU_find_next(const struct Cache &C,u_int set_ind);

//PRINTING
void printConfig();
void printResults();
void printSummary();

/***************************************************
	DUMP FUNCTION FOR GRADING/DEBUGGING
		(left in useful places but commented out)
****************************************************/
void DUMP(const struct Cache &c);


int main(){
	//step 1: read config and construct cache
	read_configuration();					  //1st read trace.config 
	printConfig();							  //print configuration info
	calc_field_size();						  //calc the field sizes in bits
	
	Cache C{}; 								  //construct cache with sizes!
	
	//step 2 read mem refs, calculate tag,index,offset and print headings
	read_references();
	
	//step 3 SIMULATION
	Simulator(C);	
	
	//Print results of the simulation
	printResults();
	printSummary();
	
	//UNCOMMENT BELOW TO SEE STATE OF CACHE AFTER SIMULATION
	// DUMP(C);
	
	return 0;
}


void read_configuration(){
	//create file object
	ifstream config_file{TRACE_CONFIG};
	//could not open? then exit
	if(!config_file){
		cerr <<"could not open "<<TRACE_CONFIG <<
			" file in current directory\n";
		exit (EXIT_FAILURE);		
	}
	
	//reads the number of sets, the set_size and line_size
	string line;
	getline(config_file, line);
	sscanf(line.c_str(), "%*[^:]: %i", &cache_size);
	getline(config_file, line);
	sscanf(line.c_str(), "%*[^:]: %i", &set_size);
	getline(config_file, line);
	sscanf(line.c_str(), "%*[^:]: %i", &line_size);
	//places them into global variables!
	
	config_file.close();
}


void calc_field_size(){
	//assumptions that cache_size and line_size are a power of two
	index_f_size = log2(cache_size); //cache size == number of sets
	offset_f_size = log2(line_size);
	tag_f_size = ADDRESS_SIZE - (index_f_size+offset_f_size);
	//tag_f_size is the number of remaining bits
}


void read_references(){
	//reference will only be saved if valid!
	
	//temporary variables for reading
	char access_char;
	string access_t, line, error_line;
	u_int hex_a;
	int size;
	
	//temp_variable for extracting tag, index, offset
	u_int tag, index, offset;
	
	//variable for line number 
	int j = 1;
	//reading the trace.dat
	while(getline(cin, line)){
		error_line.clear();
		sscanf(line.c_str(),"%c:%d:%x", &access_char, &size, &hex_a);
		//save "read"/"write" for printing later
		if(access_char == 'R')
				access_t = "read";
		else if(access_char == 'W')
				access_t = "write";
	
	
		//get the tag,index,offset from each hex_address (return by ref)
		address_extractor(hex_a,tag,index,offset);
		
		//check size = 1,2,4 or 8 
		if(((size!= 1)&&(size!= 2)&&(size!= 4)&&(size!= 8))
			||(size < 0)){
				stringstream ss;
				ss << "line " << (j) << " has illegal size "<< size <<"\n";
				//errors queue saves error messages
				errors.push(ss.str());
				//not valid! but still push
				refs.push_back(Mem_ref("null",0,0,0,0,0));
				result.push_back("xxxx");
				mem_accesses.push_back(999);
				j++;
				continue;
		}
		//check that memory is aligned(multiple of size and no overlap)
		if((hex_a % size != 0)||(size+offset > line_size)){
			stringstream ss;
			ss <<"line "<< (j) <<" has misaligned reference at address "<<
			 std::hex << hex_a << " for size " << size << "\n";
			 //save error message
			 errors.push(ss.str());
				refs.push_back(Mem_ref("null",0,0,0,0,0));
				//push into results&mem_accesses so indices match for print
				result.push_back("xxxx");
				mem_accesses.push_back(999);
				j++;
				continue;
		}
		
		//valid memory references!
		refs.push_back(Mem_ref(access_t, size,hex_a, tag, index, offset));
		//push into the other two so that indices match
		result.push_back("yyy");
		mem_accesses.push_back(999);
		j++;
	}
}

void address_extractor(u_int hex_a, u_int &tag, u_int &index, u_int &offset){
		tag = hex_a >> (index_f_size + offset_f_size);
		//(tag << (index_f_size)...) is a mask
		index = (hex_a >> offset_f_size) & ((tag << (index_f_size)) ^ (~0));
		offset = 
		  (hex_a<<(ADDRESS_SIZE-offset_f_size))>>(ADDRESS_SIZE-offset_f_size);
				
}


void Simulator(const struct Cache &C){
		//cache simulator
		
		//go through each memory reference
			//first hit or not??
			
		
		//for comparing
		u_int ind, t_tag; 
		string access_t; 
		bool isHit;
		u_int block_ind;			//remembering block hit block_ind
		u_int next2_replace;		//what block will be replaced?
		
	
	//for each reference
	for(u_int i = 0; i < refs.size(); ++i){
		//get ind and t_tag of ref
		if(refs[i].access_type == "null"){
			//reference error so do not simulate!
			//errors will be printed in printResults from errors queue
			continue;
		}
		
		//else (valid reference)
		ind = refs[i].index;
		t_tag = refs[i].tag;
		isHit = false;
		
		//DUMP HERE TO SEE CACHE CHANGE AFTER EACH REFERENCE
		// DUMP(C);
		
		//ind == set, look for t_tag in the set
		for(u_int j = 0; j < set_size; ++j){
				
				//save block index j for later
				block_ind = j;
				
			if(C.cache[ind][j].tag == t_tag&&
				C.cache[ind][j].valid_bit){
				//Hit!
				isHit = true;
				break;
				}
			//else
				//not a hit, keep searching
			
			}//exited inner for loop
			
			if(isHit){
				total_hits++;			
				result.at(i) = "hit";
				mem_accesses.at(i) = 0;
				//update LRU
				LRU_reset(C, ind, block_ind);
				//if it is a write
				if(refs[i].access_type == "write"){
					C.cache[ind][block_ind].dirty_bit = true;
				}
			}
			else{
				//Miss
				result.at(i) = "miss";
				mem_accesses.at(i) = (1);
				//get block index to replace
				next2_replace = LRU_find_next(C, ind);
				//write back if it is dirty
				if(C.cache[ind][next2_replace].dirty_bit == true){
					//1 more mem_access && set dirty to false
					C.cache[ind][next2_replace].dirty_bit = false;
					mem_accesses.at(i) = 2;
				}
				//get the new block
				C.cache[ind][next2_replace].valid_bit = true;
				//set dirty bit if it is a write
				if(refs[i].access_type == "write"){
					C.cache[ind][next2_replace].dirty_bit = true;
				}
				C.cache[ind][next2_replace].tag = t_tag;
				//update the LRU
				LRU_reset(C,ind, next2_replace);
			}
		
	}
	//if isHit == true then I have a hit at ind, t_tag!
	
}

void LRU_reset(const struct Cache &C,u_int set_ind,u_int block){
	//reset the block lru and increment the others!
	for(u_int i = 0; i < set_size; ++i){
		if(i == block){
		C.cache[set_ind][block].lru_counter = 0;
			continue;
		}
		C.cache[set_ind][i].lru_counter++;
	}
}

u_int LRU_find_next(const struct Cache &C, u_int set_ind){
	//return block number to replace
	//look for empty ones(check valid_bit), then look at largest lru_counter
	
	u_int index_next = 0;
	u_int max = C.cache[set_ind][index_next].lru_counter;
	for(u_int i = 0; i < set_size; ++i){
		//empty blocks
		if(C.cache[set_ind][i].valid_bit == false){
			index_next = i;
			break;
		}
		//largest lru_counter	
		if(C.cache[set_ind][i].lru_counter > max){
			index_next = i;
			max = C.cache[set_ind][index_next].lru_counter;
		}
	}
	//index_n holds position of an empty block or one w/ highest lru
	return index_next;
}

/**************
	PRINTERS
**************/

void printConfig(){
	//prints configuration from trace.config
	cout << "Cache Configuration\n\n   " << cache_size
		<< " " << set_size << "-way set associative entries\n   " <<
	"of line size " << line_size << " bytes\n\n\n";	
}


void printResults(){
	
	cout << "Results for Each Reference\n"<<
	"\nRef  Access Address    Tag   Index Offset Result Memrefs\n" <<
	"---- ------ -------- ------- ----- ------ ------ -------\n";
	//use j to skip references that are errors
	u_int j = 0; 
	
	for(unsigned int i = 0; i < refs.size(); ++i){
		//errors is a queue that contains all of the errors
	  if(refs.at(i).access_type == "null"){
		  ++j;
		  //prints error message
		  cerr << errors.front();
		  //pops so it won't print again
		  errors.pop();
		  //continue so that I don't print invalid data
		  continue;
	  }
		//use printf to print hexadecimal!
	   printf("%4d %6s %8x %7x %5d %6d %6s %7d\n", (i+1-j),
			refs.at(i).access_type.c_str(), refs.at(i).hex_address, 
			refs.at(i).tag, refs.at(i).index, refs.at(i).offset,
			result.at(i).c_str(), mem_accesses.at(i));
		}
	
}

void printSummary(){
	/*total_hits is a member variable
	  total_misses = total_access - total_hits;
	  total_access = refs.size();
	  hit_ratio = total_hits/accesses
	  miss_ratio = 1 - hit_ratio;
	*/
	
	//bad memory references do not cause accesses
	u_int accesses = 0;
	for(u_int i = 0; i < refs.size(); ++i){
		//count the valid number of accesses!
		if(refs[i].access_type != "null"){
			accesses++;
		}
	}
	
	//calculations
	u_int total_misses = accesses - total_hits;
	double hit_ratio = ((double)total_hits/(double)accesses);
	double miss_ratio = 1 - hit_ratio;
	
	//floating point precision = 6 
	cout <<"\n\nSimulation Summary Statistics\n-----------------------------";
	printf("\nTotal hits       : %u", total_hits);
	printf("\nTotal misses     : %u", total_misses);
	printf("\nTotal accesses   : %u", accesses);
	if(total_hits != 0){
	printf("\nHit ratio        : %2.6f", hit_ratio);
	printf("\nMiss ratio       : %2.6f\n\n", miss_ratio);
	}
	
}


/***************************************************

	DUMP FUNCTION FOR GRADING!

****************************************************/
void DUMP(const struct Cache &c){
	//uncomment to print more information!
/* 		
	cerr << "\n*********************************" << 
		"    \nFIELD SIZES\n*********************************";
	
	//check that log2() performed correctly
	cerr << "\ntag_f = " << tag_f_size << " index_f_size = " << index_f_size
		<< " offset_f_size= " << offset_f_size << endl << endl; */
		
	 cerr << "\n*********************************" << 
			"   \n ----CACHE CONTENTS----\n" << 
		 "\n*********************************\n\n";
	for(u_int i = 0; i < cache_size; ++i){
			cerr << "set " << i << "  ";
				for(u_int j = 0; j < set_size; ++j){
					//initalize to default blocks
					cerr << "  block " << j << " t" << c.cache[i][j].tag <<
						 " v" << c.cache[i][j].valid_bit <<
						 " d" << c.cache[i][j].dirty_bit <<
						 " lru " << c.cache[i][j].lru_counter;
				}
				cerr << endl;
			}
			
			
	/* cerr << "\n\n*********************************" << 
	"    \nVECTOR SIZES(should be equal)\n*********************************";
	
	//refs holds references, result hold "hit"/"miss",mem_accesses holds 0-2
	cerr << "\n\nrefs.size() = " << refs.size() << "  result.size() = " << 
	  result.size() << "  mem_accesses.size() " << mem_accesses.size()<< endl;
	 */
	
	
	/* cerr << "\n*********************************" << 
		"    \nREFERENCES\n*********************************\n";
	for(u_int i = 0; i < refs.size(); ++i){
		cerr << "ref " << i << ": " <<
			refs[i].access_type << " : " << refs[i].size << " : " 
			<< refs[i].hex_address << "   tag = " << refs[i].tag 
			<< " index = " << refs[i].index << " offs = " << 
			refs[i].offset << endl << endl;
	} */
}



