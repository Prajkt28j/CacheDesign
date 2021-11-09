/*
 * ECE563 #PROJECT1: Cache Design, Memory Hierarchy Design
 * Author: Prajakta K. Jadhav
 * UnityID: pkjadhav, 200375352   
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <memory.h>

#include "sim_cache.h"

using namespace std;

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

sim_cache <BLOCKSIZE><L1_SIZE> <L1_ASSOC><VC_NUM_BLOCKS><L2_SIZE> <L2_ASSOC><trace_file>
Example:-
    sim_cache 32 8192 4 7 262144 8 gcc_trace.txt
    argc = 8
    argv[0] = "sim_cache"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
 */

int main (int argc, char* argv[])
{
	FILE *FP;               // File handler
	char *trace_file;       // Variable that holds trace file name;
	cache_params params;    // look at sim_cache.h header file for the the definition of struct cache_params
	char rw;                // variable holds read/write type read from input file. The array size is 2 because it holds 'r' or 'w' and '\0'. Make sure to adapt in future projects
	unsigned long int addr; // Variable holds the address read from input file

	if(argc != 8)           // Checks if correct number of inputs have been given. Throw error and exit if wrong
	{
		printf("Error: Expected inputs:7 Given inputs:%d\n", argc-1);
		exit(EXIT_FAILURE);
	}

	// strtoul() converts char* to unsigned long. It is included in <stdlib.h>
	params.block_size       = strtoul(argv[1], NULL, 10);
	params.l1_size          = strtoul(argv[2], NULL, 10);
	params.l1_assoc         = strtoul(argv[3], NULL, 10);
	params.vc_num_blocks    = strtoul(argv[4], NULL, 10);
	params.l2_size          = strtoul(argv[5], NULL, 10);
	params.l2_assoc         = strtoul(argv[6], NULL, 10);
	trace_file              = argv[7];

	// Open trace_file in read mode
	FP = fopen(trace_file, "r"); 
	if(FP == NULL)
	{
		// Throw error and exit if fopen() failed
		printf("Error: Unable to open file %s\n", trace_file);
		exit(EXIT_FAILURE);
	}
	//	printf("\n ./sim_cache %lu %lu %lu %lu %lu %lu %s\n", params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);

	// Print params
	printf("  ===== Simulator configuration =====\n"
			"  BLOCKSIZE:                        %31lu\n"
			"  L1_SIZE:                          %31lu\n"
			"  L1_ASSOC:                         %31lu\n"
			"  VC_NUM_BLOCKS:                    %31lu\n"
			"  L2_SIZE:                          %31lu\n"
			"  L2_ASSOC:                         %31lu\n"
			"  trace_file:                       %31s\n", params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);
	//			"  ===================================\n\n", params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);


	//Form cache hierarchy
	dynamicCache* l2_link = NULL;
	
	//1. create L2 object
	dynamicCache Lev2(params.l2_size, params.block_size, params.l2_assoc, 2);
	if(params.l2_size)
	{
		l2_link = &Lev2;
	}
	//2. create L1 object
	dynamicCache Lev1(params.l1_size, params.block_size, params.l1_assoc, 1, params.vc_num_blocks, l2_link);

	total_mem_traffic = 0;

	char str[2];
	while(fscanf(FP, "%s %lx", str, &addr) != EOF)
	{
		rw = str[0];
		//	if (rw == 'r')
		//		printf("\n %s %lx\n", "read", addr);           // Print and test if file is read correctly
		//	else if (rw == 'w')
		//		printf("\n %s %lx\n", "write", addr);          // Print and test if file is read correctly
		/*************************************
                   Add cache code here
		 **************************************/

		//Keep CPU request data into L1 object
		Lev1.inReqTo.cpuReq_Count = totalRequests+1;
		Lev1.inReqTo.cpuReq_Type = rw;
		Lev1.inReqTo.cpuReq_MemAddr = addr;
		
		//Send CPU requests to L1 cache
		Lev1.accessCache(rw, addr);
	}

	//For simulation results disply
	Lev1.displayCacheContents();
	displaySimResults(Lev1, Lev2);

	printf("\n");
	return 0;
}

//constructor
dynamicCache::dynamicCache(ul t_size, ul t_blockSize, ul t_assoc, unsigned int level, ul t_vc_num_blocks,  dynamicCache* nextLink)
{
	//printf("::dynamicCache");
	if(!t_size) return; //Continue only if L1>0bytes 

	intializeLocals(); //Se local member variables: Miss Handling Status register to zero

	myLevel = level; 
	nextLevel = nextLink; //Connection to nextLevel cache L2/L3.. IF NULL, no nextLevel cache

	//Set cache dimentions
	m_stat.sets = t_size/(t_assoc * t_blockSize); //#sets 
	m_stat.ways = t_assoc; //#ways

	m_stat.bits_count.index = log2(m_stat.sets);//#indexbits = log2(#sets) 
	m_stat.bits_count.blockOffset = log2(t_blockSize);//#blockoffsetbits = log2(block size) 
	m_stat.bits_count.tag = 32 - m_stat.bits_count.index - m_stat.bits_count.blockOffset;//#tagbits = total#address bits - #indexbits - #blockoffsetbits 

	//Create cache stucture
	m_cache = new oneBlockMetaData* [m_stat.sets]; 
	for(ul row = 0; row < (m_stat.sets); row++){
		m_cache[row] = new oneBlockMetaData[m_stat.ways];}

	m_stat.vc_blk_count = t_vc_num_blocks;//victim cache size: number of blocks input

	//Augument VC to cache
	if( (myLevel == 1) && t_vc_num_blocks)//only L1 has augmented VC
		ownVC = new oneBlockMetaData[m_stat.vc_blk_count];
	else
		ownVC = NULL; //Disable if VC block count is zero

	//Before accessing cache, initialize cache blocks' metadata 
	for(ul i=0; i<(m_stat.sets); i++ ){
		for(ul j=0; j<(m_stat.ways); j++ ){
			m_cache[i][j].tag = 0;
			m_cache[i][j].valid = 0; //Invalid 
			m_cache[i][j].dirty = 0; //Clean
			m_cache[i][j].lruCount = j; //way0:0, way1:1, way2:2...
		}
	}

	//If VC present, initialize vc blocks' metadata
	if(ownVC != NULL){
		for(ul i=0; i<(m_stat.vc_blk_count); i++ ){
			ownVC[i].tag = 0;
			ownVC[i].valid = 0; //Invalid
			ownVC[i].dirty = 0; //Clean
			ownVC[i].lruCount = i; //vc[0]:0, vc[1]:1..
		}
	}

	/*	printf("\n -- L%d Cache constructed -- ",level );
		printf("\n [#Sets][#Ways][B'Size][SIZE]");
		printf("\n [%lu]	[%lu]	[%lu]	[%lu]", m_stat.sets, m_stat.ways, t_blockSize , (m_stat.sets*m_stat.ways*t_blockSize));
		printf("\n [Tag][Ind][B'offset]");
		printf("\n [%d] [%d]   [%d]", m_stat.bits_count.tag, m_stat.bits_count.index, m_stat.bits_count.blockOffset );
		printf("\n Cache with VC of %lu Blocks", m_stat.vc_blk_count);
	 */
}

//destructor
dynamicCache::~dynamicCache()
{
	//printf("\n ~Destruct L%d cache\n\n", myLevel);
	//Free dangling pointers: malloc error: pointer being freed was not allocated
	//delete[] m_cache;
	//if(ownVC != NULL) delete[] ownVC; 
}

void dynamicCache::accessCache(char in_reqType, ul in_addr)
{
	//printf("\n ::accessCache L%d", myLevel);

	 MATCH_C = false; //O:Miss, 1:Hit

	//Set request type(r/w), index, tag 
	setRequestValues(in_reqType, in_addr);

	if(inReqTo.type == RD){ 
		m_stat.reads++; 
	}else if(inReqTo.type == WR){	
		m_stat.writes++;
	}

	//Updates MATCH_C flag
	isBlockPresentInCache();  //invoked by L1  or L2 

	if(!MATCH_C) //Miss Handling
	{
		if(inReqTo.type == RD){
			++m_stat.r_miss;
		}else if(inReqTo.type == WR){
			++m_stat.w_miss;
		}

		allocateBlock(); //Both R/W misses result in block allocation
	}
	else{ 
		//	printf("\n ======  %c Hit in L%d =====", in_reqType, myLevel);
		//	printf("\n ::hitBlockInd %lu", hitBlockInd);

		if(inReqTo.type == RD){
			++m_stat.r_hit;
		}else if(inReqTo.type == WR){
			++m_stat.w_hit;
		}

		//1. Update LRU Counter for all blocks
		for(ul j=0; j<(m_stat.ways); ++j )
		{
			if(m_cache[inReqTo.index_val][j].lruCount < m_cache[inReqTo.index_val][hitBlockInd].lruCount)
			{
				m_cache[inReqTo.index_val][j].lruCount +=1;
			}
		}

		//2. Set found block as MRU
		m_cache[inReqTo.index_val][hitBlockInd].lruCount = 0;
		m_cache[inReqTo.index_val][hitBlockInd].valid = true;
		if(in_reqType == WR)
		{
			m_cache[inReqTo.index_val][hitBlockInd].dirty = true;
		}
	}
}

/*
 * Allocate new block(empty) in cache when request is a MISS by replacing LRU block, 
 * the requested block will be brought from next level/main mem
 */
void dynamicCache::allocateBlock()
{
	//printf("\n\n ::allocateBlock in L%d Hit/Miss[%d]", myLevel, MATCH_C);
	if(MATCH_C) return; //allocate block only on cache miss

	cacheVictimDirty = false;
	reqVCHit = false;
	cacheLRUInd = 0;
	firstEntry = false;

	if(ownVC != NULL) //VC Operation
	{
		//isBlockPresentInVC sets vcHitInd
		reqVCHit = false;
		isBlockPresentInVC();  // L1-2

		if(reqVCHit)
		{
			//printf("\n VC Hit, do swap--");

			//swap and set  MRU, SET dirty
			++m_stat.total_swap_requests; //Total swap requests //evict_to_vc
			swapBlockWithVC();
		}
		else{ //VC Miss
			//printf("\n VC Miss--");

			//case 1: NO L2, get from M.M
			if(nextLevel == NULL) 
			{
				//printf("\n 1.-- Inc total_mem_traffic[%lu] ", total_mem_traffic);
				++total_mem_traffic; 

				//printf("\n 2.Find LRU");
				findLRUInCache();

				if(!firstEntry){
					//printf("\n 3.Put victim LRU into VC");
					++m_stat.total_swap_requests; 
					addVictimBlockInVC();
				}

				//printf("\n 4.Add new block metadata, set tag,valid,dirty");
				m_cache[inReqTo.index_val][cacheLRUInd].valid = true;
				m_cache[inReqTo.index_val][cacheLRUInd].tag = inReqTo.tag_val;
				if(inReqTo.type == WR)
				{
					m_cache[inReqTo.index_val][cacheLRUInd].dirty = true;
				}else{
					m_cache[inReqTo.index_val][cacheLRUInd].dirty = false;
				}

				//Before seting as MRU, Update LRU Counters for all remaining blocks
				//printf("\n 5.Before seting as MRU, Update LRU Counters for all remaining blocks");
				for(ul i = 0; i<m_stat.ways; i++){
					if(m_cache[inReqTo.index_val][i].lruCount < (m_cache[inReqTo.index_val][cacheLRUInd].lruCount))
					{
						m_cache[inReqTo.index_val][i].lruCount +=1;
					}
				}

				//printf("\n 6. set MRU for newly added block");
				//Set MRU block, before this 'cacheLRUInd' was index of LRU ,
				m_cache[inReqTo.index_val][cacheLRUInd].lruCount = 0; //set as MRU
				//printf("\n Success ---->New Block Added in L%d ---", myLevel);

			}
			else if(nextLevel != NULL)//case 2: L1,VC present, No VC, ask M.M  
			{
				//Before allocating new block, put L1-LRU in VC
				//printf("\n 1.Find LRU");
				findLRUInCache();

				if(!firstEntry){
					//printf("\n 2.Put victim LRU into nextLevel");
					++m_stat.total_swap_requests; 
					addVictimBlockInVC();
				}

				//printf("\n 4.Add new block metadata, set tag,valid,dirty");
				m_cache[inReqTo.index_val][cacheLRUInd].valid = true;
				m_cache[inReqTo.index_val][cacheLRUInd].tag = inReqTo.tag_val;
				if(inReqTo.type == WR)
				{
					m_cache[inReqTo.index_val][cacheLRUInd].dirty = true;
				}else{
					m_cache[inReqTo.index_val][cacheLRUInd].dirty = false;
				}

				//Before seting as MRU, Update LRU Counters for all remaining blocks
				//printf("\n 5.Before seting as MRU, Update LRU Counters for all remaining blocks");
				for(ul i = 0; i<m_stat.ways; i++){
					if(m_cache[inReqTo.index_val][i].lruCount < (m_cache[inReqTo.index_val][cacheLRUInd].lruCount))
					{
						m_cache[inReqTo.index_val][i].lruCount +=1;
					}
				}

				//printf("\n 6. set MRU for newly added block");
				//Set MRU block, before this 'cacheLRUInd' was index of LRU ,
				m_cache[inReqTo.index_val][cacheLRUInd].lruCount = 0; //set as MRU

				//printf("\n Success ---->New Block Added in L%d ---", myLevel);

				//printf("\n *** 7. Issue READ request to nextLev ***, myLev:L%d", myLevel);
				(*nextLevel).accessCache(RD, inReqTo.mem_addr); //L2-1
			}
		}
	}
	else if( ownVC == NULL) //case3: L1's VC & L2 both are Absent, L1 to M.M directly
	{
		if(nextLevel == NULL) 
		{
			//printf("\n 1.-- Inc total_mem_traffic[%lu] ", total_mem_traffic);
			++total_mem_traffic; 

			//printf("\n 2.Find LRU");
			cacheVictimDirty = false;
			findLRUInCache();

			if(cacheVictimDirty && (ownVC == NULL))
			{
				//printf("\n 3.WriteBack **DIRTY** victim LRU into MM --- ");
				++m_stat.wb_to_next;
				++total_mem_traffic; 
			}

			//printf("\n 4.Add new block metadata, set tag,valid,dirty");
			m_cache[inReqTo.index_val][cacheLRUInd].valid = true;
			m_cache[inReqTo.index_val][cacheLRUInd].tag = inReqTo.tag_val;
			if(inReqTo.type == WR)
			{
				m_cache[inReqTo.index_val][cacheLRUInd].dirty = true;
			}else{
				m_cache[inReqTo.index_val][cacheLRUInd].dirty = false;
			}

			//Before seting as MRU, Update LRU Counters for all remaining blocks
			//printf("\n 5.Before seting as MRU, Update LRU Counters for all remaining blocks");
			for(ul i = 0; i<m_stat.ways; i++){
				if(m_cache[inReqTo.index_val][i].lruCount < (m_cache[inReqTo.index_val][cacheLRUInd].lruCount))
				{
					m_cache[inReqTo.index_val][i].lruCount +=1;
				}
			}
			//printf("\n 6. set MRU for newly added block");
			//Set MRU block, before this 'cacheLRUInd' was index of LRU ,
			m_cache[inReqTo.index_val][cacheLRUInd].lruCount = 0; //set as MRU

			//printf("\n Success New Block Added in L%d , NO VC---", myLevel);
		}
		else if(nextLevel != NULL) //case4: L1&L2 Present, L1-VC Absent, 
		{
			findLRUInCache();

			if( cacheVictimDirty && (ownVC == NULL)) //Write-back dirty block
			{
				++m_stat.wb_to_next;

				//Re-constitute L2 block address  
				ul L2_tag = ( ( (m_cache[inReqTo.index_val][cacheLRUInd].tag) << (m_stat.bits_count.index)) | inReqTo.index_val ) << (m_stat.bits_count.blockOffset);

				//printf("\n *** Issue WRITE request to nextLev ***, myLev:L%d", myLevel);
				(*nextLevel).accessCache(WR, L2_tag); //L2-1
			}

			//printf("\n *** 3. Issue READ request to nextLev ***, myLev:L%d", myLevel);
			(*nextLevel).accessCache(RD, inReqTo.mem_addr); //L2-1

			//l1 should set its own tag not derived from l2
			setRequestValues(inReqTo.cpuReq_Type, inReqTo.cpuReq_MemAddr);

			//printf("\n 4.----- Add new block metadata, set tag,valid,dirty");
			m_cache[inReqTo.index_val][cacheLRUInd].valid = true;
			m_cache[inReqTo.index_val][cacheLRUInd].tag = inReqTo.tag_val;
			if(inReqTo.type == WR)
			{
				m_cache[inReqTo.index_val][cacheLRUInd].dirty = true;
			}else{
				m_cache[inReqTo.index_val][cacheLRUInd].dirty = false;
			}

			//Before seting as MRU, Update LRU Counters for all remaining blocks
			//printf("\n 5.Before seting as MRU, Update LRU Counters for all remaining blocks");
			for(ul i = 0; i<m_stat.ways; i++){
				if(m_cache[inReqTo.index_val][i].lruCount < (m_cache[inReqTo.index_val][cacheLRUInd].lruCount))
				{
					m_cache[inReqTo.index_val][i].lruCount +=1;
				}
			}
			//printf("\n 6. set MRU for newly added block");
			//Set MRU block, before this 'cacheLRUInd' was index of LRU ,
			m_cache[inReqTo.index_val][cacheLRUInd].lruCount = 0; //set as MRU
		}
	}
}


/* VC is small, full assoc cache which enhances capacity of its primary cache(L1/L2/..) 
 * prim.cache & its VC are mutually exclusive
 * VC operations:
 * 1. search for CPU requested block 
 * 2. request VC Miss?-Y, (L1 gets(issue r/w, allocate blk) tht block from L2)
 * 		what abt New victim block from primCache ? -> 
 * 		Overflow it to VC at 'LRU' and make it MRU and update remaining LRUs
 * 3. request VC Hit? 
 * 		the L1 cache and victim cache “swap” blocks. 
 *  	VC sends the requested block to the L1 and 
 *  	the L1 sends its newly-evicted block to the VC
 *   	at the location just vacated by the requested block. 
 */

//cache's LRU will be added to VC
void dynamicCache::addVictimBlockInVC()
{
	//printf("\n ::addVictimBlockInVC, L%d ", myLevel);
		
	if(ownVC == NULL) return; //sanity

	vcVictimInd = 0;
	bool needVCVictimWB = false;

	//printf("\n FIND VC-LRU, vcVictimInd[%lu]", vcVictimInd);
	ul largeLRUCount = ownVC[0].lruCount;
	for(ul j=1; j<(m_stat.vc_blk_count); ++j )
	{
		if(largeLRUCount < ownVC[j].lruCount)
		{
			largeLRUCount = ownVC[j].lruCount;
			vcVictimInd = j; 
		}
	}

	if((ownVC[vcVictimInd].dirty == true ) &&
			(ownVC[vcVictimInd].valid == true))
	{
		needVCVictimWB = true;
	}

	// Before adding new block at LRUInd, check if VC-Victim block needs write-back?
	// If yes, first perform write back then add new block at LRUInd of VC
	if(needVCVictimWB)
	{
		++m_stat.wb_to_next; // inc for vc to nextLevel(L2/MM) wb, number writebacks from L1/VC:
		//printf("\n Increased wb count [%lu] for victim[%lu]", m_stat.wb_to_next, vcVictimInd);

		//issue req to next level
		if( nextLevel != NULL) //l1
		{
			//Re-constitute L2 block address  
			ul L2_tag = (ownVC[vcVictimInd].tag) << (m_stat.bits_count.blockOffset);

			//printf("\n ---WriteBack to nextLevel from 'VC' of L%d to L2 BlkAddr[%lx]---", myLevel, L2_tag );
			(*nextLevel).accessCache(WR, L2_tag);
		}
		else //VC to Main Mem directly
		{
			++total_mem_traffic;
			//printf("\n 4.-- Inc in VCto MM total_mem_traffic[%lu] ", total_mem_traffic);
		}
	}

	//printf("\n === L%d victim block to be added in VC ====", myLevel);
	//printf("\n[set][way][Tag][LRU][D]");
	/*printf("\n[%lu][%lu][%lx][%lu][%d]",inReqTo.index_val, cacheLRUInd,
			m_cache[inReqTo.index_val][cacheLRUInd].tag,
			m_cache[inReqTo.index_val][cacheLRUInd].lruCount, 
			m_cache[inReqTo.index_val][cacheLRUInd].dirty );
	 */

	//3. Update LRU Counter for all blocks, before setting new block as MRU
	for(ul i = 0; i<m_stat.vc_blk_count; i++){
		if(ownVC[i].lruCount < ownVC[vcVictimInd].lruCount)
		{
			ownVC[i].lruCount +=1;
		}
	}

	//Reconstitute tag for VC Block, VC is fully assoc hence no index bits
	ul newTag = ( ( (m_cache[inReqTo.index_val][cacheLRUInd].tag) << m_stat.bits_count.index ) |  inReqTo.index_val );
	//printf("\n == newTag for VC [%lx]", newTag);
	ownVC[vcVictimInd].tag = newTag;
	ownVC[vcVictimInd].dirty = m_cache[inReqTo.index_val][cacheLRUInd].dirty;
	ownVC[vcVictimInd].valid = m_cache[inReqTo.index_val][cacheLRUInd].valid;
	ownVC[vcVictimInd].lruCount = 0; //set MRU

	//printf("\n --------Added cache victim block in VC at[%lu]", vcVictimInd);
	//printVCContents();
}

/* CPU request-> VC Hit? 
 *	L1 cache LRU blk and victim cache's 'found' blocks are “swapped”  
 *	VC sends the requested block to the L1 and 
 *  the L1 sends its newly-evicted block to the VC
 *  at the location just vacated by the requested block. 
 * 
 *  Note: After swapping the blocks, the dirty bit of block X in CACHE 
 *  may need to be updated (based on whether the original request
 *   to block X is a read or a write).
 */

void dynamicCache::swapBlockWithVC()
{
	//printf("\n ::swapBlockWithVC ");

	//sanity
	if(ownVC == NULL ) return;

	// findLRUInCache sets cacheLRUInd
	findLRUInCache();

	//printf("\n ---- L%d Victim Block to be swapped---", myLevel);
	//printf("\n[set][way][Tag][LRU][D]");
	/*	//printf("\n[%lu][%lu][%lx][%lu][%d]",inReqTo.index_val, cacheLRUInd,
			m_cache[inReqTo.index_val][cacheLRUInd].tag,
			m_cache[inReqTo.index_val][cacheLRUInd].lruCount, 
			m_cache[inReqTo.index_val][cacheLRUInd].dirty );
	 */

	//Reconstitute cache-LRU tag to put into VC 
	ul victimTag = (((m_cache[inReqTo.index_val][cacheLRUInd].tag) << m_stat.bits_count.index) | inReqTo.index_val);
	bool victimDirty = m_cache[inReqTo.index_val][cacheLRUInd].dirty;
	//printf("\n Reconstituted VC-Tag[%lx]L1-Victim-Dirty[%d]", victimTag, victimDirty);


	//printf("\n ---- VC Vict Block for swap---" );
	//printf("\n[Ind][Tag][LRU][D]");
	//printf("\n[%lu][%lx][%lu][%d]", vcHitInd, ownVC[vcHitInd].tag, ownVC[vcHitInd].lruCount, ownVC[vcHitInd].dirty );

	//Put VC Swap-Block data into cache victim Block
	m_cache[inReqTo.index_val][cacheLRUInd].valid = true;
	m_cache[inReqTo.index_val][cacheLRUInd].tag = ((ownVC[vcHitInd].tag) >> (m_stat.bits_count.index));
	m_cache[inReqTo.index_val][cacheLRUInd].dirty = ownVC[vcHitInd].dirty;

	//irrespective of whats VC Found blck clean/D status is, if new req was of write
	if(inReqTo.type == WR) 
	{
		m_cache[inReqTo.index_val][cacheLRUInd].dirty = true;	
	} 

	//2. Update LRU Counter for all blocks
	for(ul i = 0; i<m_stat.ways; i++){
		if(m_cache[inReqTo.index_val][i].lruCount < (m_cache[inReqTo.index_val][cacheLRUInd].lruCount))
		{
			m_cache[inReqTo.index_val][i].lruCount +=1;
		}
	}

	//check linking
	m_cache[inReqTo.index_val][cacheLRUInd].lruCount = 0; //set as MRU

	//Put cache LRU block into VC 
	ownVC[vcHitInd].tag = victimTag;
	ownVC[vcHitInd].dirty = victimDirty;
	ownVC[vcHitInd].valid = true;

	//update LRU Counter for all blocks before making VC-Swap block MRU
	for(ul j=0; j<(m_stat.vc_blk_count); ++j )
	{
		if(ownVC[j].lruCount < (ownVC[vcHitInd].lruCount))
		{
			ownVC[j].lruCount +=1;
		}
	}

	// set the newly added block by swapping to MRU 
	ownVC[vcHitInd].lruCount = 0;

	//printf("\n swapping done ======");
	++m_stat.vc_success_swaps;

	//printVCContents();
	//printCacheContents();
}


void dynamicCache::findLRUInCache()
{
	//printf("\n start findLRUInCache in L%d", myLevel);

	cacheVictimDirty = false;
	cacheLRUInd = 0;
	firstEntry = false;

	ul largeLRUCount = m_cache[inReqTo.index_val][0].lruCount;

	//find LRU
	for(ul j=1; j<(m_stat.ways); ++j )
	{
		if(largeLRUCount < m_cache[inReqTo.index_val][j].lruCount)
		{
			largeLRUCount = m_cache[inReqTo.index_val][j].lruCount;
			cacheLRUInd = j;
		}
	}

	if(m_cache[inReqTo.index_val][cacheLRUInd].dirty == true)
	{
		cacheVictimDirty = true;
	}

	if( (m_cache[inReqTo.index_val][cacheLRUInd].tag == 0) &&
			(m_cache[inReqTo.index_val][cacheLRUInd].dirty == false) &&
			(m_cache[inReqTo.index_val][cacheLRUInd].valid == false) )
	{
		firstEntry = true;
	}
	//printf("\n end L%d LRU-> Ind[%lu]Dirty[%d]", myLevel, cacheLRUInd, cacheVictimDirty);
}


void dynamicCache::intializeLocals()
{
	MATCH_C = false;
	cacheVictimDirty = false;
	firstEntry = false;
	reqVCHit = false;

	vcHitInd = 0;
	hitBlockInd = 0; //cache hit index
	cacheLRUInd = 0; //victim

	m_stat.reads = 0; 
	m_stat.r_miss = 0;
	m_stat.writes = 0;
	m_stat.w_miss = 0;
	m_stat.wb_to_next = 0;
	m_stat.total_swap_requests = 0; 

	//cache dimentions
	m_stat.sets = 0; 
	m_stat.ways = 0;
	m_stat.vc_blk_count = 0;

	m_stat.bits_count.tag = 0;  
	m_stat.bits_count.index = 0;
	m_stat.bits_count.blockOffset = 0;

	m_stat.vc_miss = 0;
	m_stat.vc_success_swaps = 0;

	m_stat.r_hit = 0;
	m_stat.w_hit = 0;

	inReqTo.reqCount = 0;

	inReqTo.cpuReq_Count = 0;
	inReqTo.cpuReq_Type = 0;
	inReqTo.cpuReq_MemAddr = 0;

}

void displaySimResults(dynamicCache &L1, dynamicCache &L2)
{

	//L1+VC Miss Rate=(L1 read misses + L1 write misses – swaps)/(L1 reads + L1 writes)
	float MR_l1_vc = (static_cast<float> (L1.m_stat.r_miss + L1.m_stat.w_miss - L1.m_stat.vc_success_swaps)/static_cast<float> ( L1.m_stat.reads + L1.m_stat.writes));

	//L1 Swap Request Rate=(swap requests)/(L1 reads + L1 writes)
	float L1_SRR = ( static_cast<float>(L1.m_stat.total_swap_requests ) / static_cast<float>( L1.m_stat.reads + L1.m_stat.writes) );

	bool L2_Present = false;
	if(L1.nextLevel != NULL) { L2_Present = true; }

	//L2 Miss Rate(from standpoint of stalling the CPU)= MRL2 =(L2 read misses)/(L2 reads)
	float MR_l2_vc = (static_cast<float> (L2.m_stat.r_miss)/ (static_cast<float> ( L2.m_stat.reads) ) );

	//L1 Results
	printf("\n===== Simulation results =====");
	printf("\na. number of L1 reads:                       %lu", 	L1.m_stat.reads);
	printf("\nb. number of L1 read misses:                 %lu", 	L1.m_stat.r_miss);

	printf("\nc. number of L1 writes:                      %lu", 	L1.m_stat.writes);
	printf("\nd. number of L1 write misses:                %lu", 	L1.m_stat.w_miss);

	printf("\ne. number of swap requests:                  %lu", 	L1.m_stat.total_swap_requests); 
	printf("\nf. swap request rate:                        %0.4f",  L1_SRR);
	printf("\ng. number of swaps:                          %lu", 	L1.m_stat.vc_success_swaps);
	printf("\nh. combined L1+VC miss rate:                 %0.4f",  MR_l1_vc);
	printf("\ni. number writebacks from L1/VC:             %lu", 	L1.m_stat.wb_to_next);


	//L2 Results
	printf("\nj. number of L2 reads:                       %lu",   (L2_Present ? (L2.m_stat.reads) : 0));
	printf("\nk. number of L2 read misses:                 %lu",   (L2_Present ? (L2.m_stat.r_miss) : 0));
	printf("\nl. number of L2 writes:                      %lu",   (L2_Present ? (L2.m_stat.writes) : 0));
	printf("\nm. number of L2 write misses:                %lu",   (L2_Present ? (L2.m_stat.w_miss) : 0));
	printf("\nn. L2 miss rate:                             %0.4f", (L2_Present ? MR_l2_vc : 0));
	printf("\no. number of writebacks from L2:             %lu",   (L2_Present ? (L2.m_stat.wb_to_next) : 0)); 

	printf("\np. total memory traffic:                     %lu", total_mem_traffic);

}


void dynamicCache::setRequestValues(char type, ul in_addr)
{
	//printf("\n ::setRequestValues, L%d ", myLevel);
	inReqTo.reqCount += 1;
	inReqTo.type = type;
	inReqTo.mem_addr = in_addr;
	inReqTo.tag_val = inReqTo.mem_addr >> (m_stat.bits_count.index + m_stat.bits_count.blockOffset);

	ul temp = inReqTo.tag_val << (m_stat.bits_count.index + m_stat.bits_count.blockOffset); //insert 0s in place of ind and boffset
	temp = in_addr - temp; //only index and offset
	inReqTo.index_val = temp >> m_stat.bits_count.blockOffset;

	//printf("\n mem_addr: %lx", inReqTo.mem_addr);
	//	printf("\n tag_val: %lx", inReqTo.tag_val);
	//	printf("\n index_val : %lx", inReqTo.index_val );
}

void dynamicCache::isBlockPresentInCache()
{
	//printf("\n ::isBlockPresentInCache, L%d ", myLevel);
	
	MATCH_C = false; //Default Miss
	hitBlockInd = 0;

	for(ul j=0; j<(m_stat.ways); j++ )
	{
		if((m_cache[inReqTo.index_val][j].valid == true) && 
				(m_cache[inReqTo.index_val][j].tag == inReqTo.tag_val) )
		{
			MATCH_C = true;
			hitBlockInd = j;
		}
	}
}

void dynamicCache::isBlockPresentInVC()
{
	//printf("\n L%d Cache isBlockPresentInVC", myLevel );
	
	reqVCHit = false;
	ul tag = (( inReqTo.tag_val << m_stat.bits_count.index) | inReqTo.index_val);

	for(ul j=0; j<(m_stat.vc_blk_count); j++ )
	{
		if( (ownVC[j].valid == true) && 
				(ownVC[j].tag ==tag) )
		{
			reqVCHit = true;
			vcHitInd = j;
		}
	}
}

void dynamicCache::displayCacheContents()
{
	char dirty = ' ';

	//Print L1 Content
	printf("\n===== L1 contents =====");
	for(ul i=0; i<(m_stat.sets); i++ ){
		printf("\n set %3lu: ",i);
		if(m_stat.ways > 1)
		{
			sortColumns(m_stat.ways, m_cache[i]); //l1
		}
				
		for(ul j=0; j<(m_stat.ways); j++ )
		{
			dirty = ' ';
			if(m_cache[i][j].tag != 0)
			{
				if(m_cache[i][j].dirty){ dirty = 'D';}
				printf("  %lx %c", m_cache[i][j].tag, dirty);
			}
		}
	}
	printf("\n");

	if(ownVC != NULL)
	{
		displayVCContents();
		printf("\n");
	}

	if(nextLevel == NULL) return;

	//Print L2 Content
	printf("\n===== L2 contents =====");
	for(ul i=0; i<(nextLevel->m_stat.sets); i++ ){
		printf("\n set %3lu: ",i);
		
		if(nextLevel->m_stat.ways > 1)
		{
			sortColumns((nextLevel->m_stat.ways), nextLevel->m_cache[i]);
			//sort(nextLevel->m_cache[i], m_cache[i]+(nextLevel->m_stat.ways), compare);
		}
		
		for(ul j=0; j<(nextLevel->m_stat.ways); j++ )
		{
			dirty = ' ';
			if(nextLevel->m_cache[i][j].tag != 0)
			{
				if(nextLevel->m_cache[i][j].dirty){ dirty = 'D';}
				printf("  %lx %c", nextLevel->m_cache[i][j].tag, dirty);
			}
		}
	}
	printf("\n");
}


void dynamicCache::displayVCContents()
{
	if(ownVC == NULL) return;
	char dirty;

	sortColumns(m_stat.vc_blk_count, ownVC);
	
	//Print VC Content
	printf("\n===== VC contents =====");// vc_blk_count[%lu]", m_stat.vc_blk_count);
	printf("\n set %3d: ",0);
	for(ul i=0; i<(m_stat.vc_blk_count); i++ )
	{
		dirty = ' ';
		if(ownVC[i].dirty){ dirty = 'D'; }
		if(ownVC[i].tag != 0){ printf("  %lx %c", ownVC[i].tag, dirty); }
	}
}

/*
 *  sortColumns method is called by displayCacheContents to provide LRU Counter value based sorted 
 *  L1/VC/L2 cache contents i.e. to display MRU block in fist column and LRU Block in lat column
 */
void dynamicCache::sortColumns(ul size, oneBlockMetaData *one_set )
{
	oneBlockMetaData *ab = new oneBlockMetaData ;
	
	for( ul i = 0; i < size; ++i) 
	{
		for(ul j = i + 1; j < size; ++j)
		{
			if ( ((*(one_set + i)).lruCount) > ((*(one_set + j)).lruCount)) 
			{
				//memcpy (dest_struct, source_struct, sizeof (*dest_struct));
				memcpy(ab, (one_set + i), sizeof(oneBlockMetaData));
				memcpy((one_set + i), (one_set + j), sizeof(oneBlockMetaData));
				memcpy((one_set + j), ab, sizeof(oneBlockMetaData));
			}
		}
	}
}


