/*
 * ECE563 #PROJECT1: Cache Design, Memory Hierarchy Design
 * Author: Prajakta K. Jadhav
 * UnityID: pkjadhav, 200375352   
 */

#ifndef SIM_CACHE_H
#define SIM_CACHE_H

typedef struct cache_params{
	unsigned long int block_size;
	unsigned long int l1_size;
	unsigned long int l1_assoc;
	unsigned long int vc_num_blocks;
	unsigned long int l2_size;
	unsigned long int l2_assoc;
}cache_params;

//forward declaration
class dynamicCache;
void displaySimResults(dynamicCache &L1, dynamicCache &L2);

struct bitInfo{
	unsigned short tag;  //2byte
	unsigned short index;
	unsigned short blockOffset;
};

typedef unsigned long int ul; //8byte on 64-bit target machine

typedef struct perBlock {
	ul tag;
	ul lruCount; //e.g. 0:MRU, 1:MRU-1,  .. 3:LRU
	bool valid;
	bool dirty;
} oneBlockMetaData;

typedef struct accessInfo {
	ul cpuReq_Count;
	ul cpuReq_Type;
	ul cpuReq_MemAddr;

	//local, specific to l1/l2
	ul reqCount;
	char type;
	ul mem_addr;
	ul block_addr;

	ul tag_val;
	ul index_val;
	ul blockOffset_val;
} accessReq;


typedef struct missHandlingStatusReg {
	bitInfo bits_count;

	//cache dimentions for respective cache (L1/L2/..)
	ul sets; //total sets count 
	ul ways;
	ul vc_blk_count;

	ul reads; //4byte
	ul r_miss;
	ul writes;
	ul w_miss;

	ul wb_to_next; //Writeback to nextLevel mem L2/L3/M.M count
	ul total_swap_requests; //Total requests of Cache Victim block(LRU) to VC count

	ul vc_miss; //Request miss in VC count
	ul vc_success_swaps; //successful swaps count

	//additional
	ul r_hit;
	ul w_hit;
} localStatusReg;


// --- GLOBAL VARIABLES ----
static const char WR= 'w';
static const char RD = 'r';
static ul totalRequests = 0;
static ul total_mem_traffic = 0;

class dynamicCache{

public:
	//Construct cache object using BLOCK_SIZE, SIZE, ASSOC, LEVEL, VC_BLOCK_COUNT, LINK_TO_NEXT_LEVEL_CACHE 
	dynamicCache(ul, ul, ul, unsigned int, ul vc_num_blocks = 0, dynamicCache* nextLevel = NULL); 
	virtual ~dynamicCache(); 

	//Member Functions
	void intializeLocals(void);

	void accessCache(char, ul);
	void setRequestValues(char, ul);

	void isBlockPresentInCache(void);
	void isBlockPresentInVC(void);
	void allocateBlock(void);
	void findLRUInCache(void);

	void addVictimBlockInVC(void);
	void swapBlockWithVC(void);

	//For simulation results
	void displayCacheContents();
	void displayVCContents();
	void sortColumns(ul, oneBlockMetaData *one_set);

	//Memeber variables
	dynamicCache      * nextLevel; //L2/L3/..
	oneBlockMetaData  * ownVC; //Link to augmented victim cache structure
	oneBlockMetaData ** m_cache; //Link to cache structure made of metadata blocks

	unsigned int myLevel; //Identifies level of cache L'1', L'2'..

	accessReq inReqTo; //Input request includes type, tag, index
	localStatusReg m_stat; //Miss Handling Register

	ul hitBlockInd; //Index of CacheBlock Hit for input req
	ul cacheLRUInd; //Cache LRU block Index: victim

	//variables for swaps:
	ul vcHitInd;//Index of VCblock Hit for swapping with cacheVictimInd cacheblock
	ul vcVictimInd; //LRU Block of VC, if dirty, write back to nextLevel
	
	bool MATCH_C; //True: HIT, False:MISS
	bool firstEntry; // True if request block is first ever entry in the cacheBlock 
	bool reqVCHit; // True if requestblock hit in VC 
	bool cacheVictimDirty; // True if victim block is dirty, eligible for writeback 
		
};

#endif
