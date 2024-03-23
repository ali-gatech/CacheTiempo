#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <climits>
#include <cstring>
#include <cmath>
#include <vector>

#include "cache.h"

unsigned long long int access_time = 1;

using namespace std;
/////////////////////////////////////////////////////////////////////////////////////
// ---------------------- DO NOT MODIFY THE PRINT STATS FUNCTION --------------------
/////////////////////////////////////////////////////////////////////////////////////

void cache_print_stats(Cache* c, char* header){
	double read_mr =0;
	double write_mr =0;
	if (c->stat_read_access) {
		read_mr = (double)(c->stat_read_miss) / (double)(c->stat_read_access);
	}

	if (c->stat_write_access) {
		write_mr = (double)(c->stat_write_miss) / (double)(c->stat_write_access);
	}

	printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
	printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
	printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
	printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
	printf("\n%s_READ_MISS_PERC  \t\t : %10.3f", header, 100*read_mr);
	printf("\n%s_WRITE_MISS_PERC \t\t : %10.3f", header, 100*write_mr);
	printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);
	printf("\n");
}


/////////////////////////////////////////////////////////////////////////////////////
// Allocate memory for the data structures 
// Initialize the required fields 
/////////////////////////////////////////////////////////////////////////////////////

Cache* cache_new(uint64_t size, uint64_t assoc, uint64_t linesize, uint64_t repl_policy)
{

	Cache* cache_mem = (Cache*) calloc (1, sizeof(Cache));
	cache_mem->num_sets = (size/linesize)/assoc;
	Cache_Set* sets = (Cache_Set*) calloc (cache_mem->num_sets, sizeof(Cache_Set));
	cache_mem->cache_sets = sets;
	for (int i = 0 ; i < cache_mem->num_sets; i++)
	{
		sets[i].ways = assoc;
		Cache_Line* line = (Cache_Line*) calloc (assoc, sizeof(Cache_Line));
		sets[i].cache_ways = line;
	}
	cache_mem->replace_policy = repl_policy;
	cache_mem->cache_sets = sets;
	return cache_mem;

}

/////////////////////////////////////////////////////////////////////////////////////
// Return HIT if access hits in the cache, MISS otherwise 
// If is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
/////////////////////////////////////////////////////////////////////////////////////

bool cache_access(Cache* c, Addr lineaddr, uint32_t is_write, uint32_t core_id){

	uint32_t set_index = lineaddr & (c->num_sets-1);
	uint32_t tag_index = lineaddr;
	if(is_write)
	{
		c->stat_write_access++;
	}
	else
	{
		c->stat_read_access++;
	}

	for (int i = 0; i < c->cache_sets[0].ways; i++)
	{
		
		Cache_Line* line_access = &c->cache_sets[set_index].cache_ways[i];
		if (line_access->tag == tag_index && line_access->valid)
		{
			line_access->LAT = access_time++;
			line_access->freq++;
			if (is_write)
				line_access->dirty = true;
			return true;
		}
	}
	
	if(is_write)
	{
		c->stat_write_miss++;
	}
	else
	{
		c->stat_read_miss++;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// Install the line: determine victim using replacement policy 
// Copy victim into last_evicted_line for tracking writebacks
/////////////////////////////////////////////////////////////////////////////////////

void cache_install(Cache* c, Addr lineaddr, uint32_t is_write, uint32_t core_id){

	uint32_t set_index = lineaddr & (c->num_sets-1);
	uint32_t tag_index = lineaddr;

	uint32_t victim_index = cache_find_victim(c, set_index, core_id);

	Cache_Line* new_line = &c->cache_sets[set_index].cache_ways[victim_index];
	if (new_line->dirty)
	{
		c->stat_dirty_evicts++;
	}
	c->evict_line = *new_line;
	new_line->LAT = access_time++;
	new_line->tag = tag_index;
	new_line->valid = true;
	new_line->freq = 1;
	if (is_write)
	{
		new_line->dirty = true;
	}
	else
	{
		new_line->dirty = false;
	}
}
	

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////
uint32_t cache_find_victim(Cache *c, uint32_t set_index, uint32_t core_id){

	uint32_t min_index = 0;
	if(c->replace_policy == 0)
	{
		unsigned long long int min_access_time = c->cache_sets[set_index].cache_ways[0].LAT;

		for (int i = 0; i < c->cache_sets[0].ways; i++)
		{
			if(!c->cache_sets[set_index].cache_ways[i].valid)
			{
				min_index = i;
				return min_index;
			}
			else if (c->cache_sets[set_index].cache_ways[i].LAT < min_access_time)
			{
				min_index = i;
				min_access_time = c->cache_sets[set_index].cache_ways[i].LAT;
			}
		}
	}

	else if(c->replace_policy == 1)
	{
		unsigned long long int freq = c->cache_sets[set_index].cache_ways[0].freq;
		vector <uint32_t> LFU;
		for (int i = 0; i < c->cache_sets[0].ways; i++)
		{
			if (!c->cache_sets[set_index].cache_ways[i].valid)
			{
				min_index = i;
				return min_index;
			}
			else if (c->cache_sets[set_index].cache_ways[i].freq < freq)
			{
				freq = c->cache_sets[set_index].cache_ways[i].freq;
			}
		}
		for (int i = 0 ; i < c->cache_sets[0].ways; i++)
		{
			if (c->cache_sets[set_index].cache_ways[i].freq == freq)
			{
				LFU.push_back(i);
			}
		}
		if (LFU.size() == 1)
		{
			min_index = LFU[0];
		}
		else if (LFU.size() > 1)
		{
			unsigned long long int max_access_time = c->cache_sets[set_index].cache_ways[LFU[0]].LAT;
			min_index = LFU[0];
			for (int i = 1 ; i < LFU.size(); i++)
			{	
				if (c->cache_sets[set_index].cache_ways[LFU[i]].LAT > max_access_time)
				{
					max_access_time = c->cache_sets[set_index].cache_ways[LFU[i]].LAT;
					min_index = LFU[i];
				}

			}
		}
	}

	return min_index;
}