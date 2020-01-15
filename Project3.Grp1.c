
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define COLD_MISS 0
#define CAPA_MISS 1
#define CONF_MISS 2
#define NO_MISS 3
#define FA 0
#define NUM_OF_STREAM 4
#define MAX_STREAM_BUFFER 8
#define MOST_RECENT_USE 3
#define STRIDE_PREFETCHER 0
#define STREAM_BUFFER 1
#define CORRELATING_PREFETCHER 2
#define CONTENT_PREFETCHER 3
#define MAX_THT_ARRY 4
#define MAX_PHT_ARRY 4
#define NUM_OF_STRIDE 3


/// -  Structure of cache data
struct cacheData
{
    int cacheLines;
    int *data;
    int cacheLineSize;
    int tag;
    int valid;
    int set;
    int lru;
};

/// - Structure of victim cache data
struct victimData
{
    int tag;
};
/// - tag and index data buffer for stride/stream buffer
struct sbAddrData
{
    int tag;
    int setNum;
};
/// - Structure of stream buffer / stride prefetcher data
struct streamBufferData
{
    struct sbAddrData addrData[8];
    int lru;
};
/// - Tag data buffer for tag history table
struct thtData
{
    int tag[4];
};
/// - tag data buffer for pattern history table
struct phtAddrData
{
    int tag[2];
};
/// - index buffer for pattern history table
struct patternHistrofyBuffer
{
    struct phtAddrData index[4];
};

/// - This function re-order LRU count in cache set.
void reOrderLru(int lineNum, int associativity, struct cacheData *my_cache, int setNum)
{
    int i=0, j=0, startLine=setNum*associativity;/// - start block number always be setNumber * associativity.
    int temp = 0;
    
    if ( my_cache[lineNum].lru == 0 )   /// - If hit and it was LRU
    {
        my_cache[lineNum].lru = associativity;
        for(i=startLine; i<startLine+associativity; i++)
        {
            for(j = associativity -1; j > -1; j--)
            {
                if ( my_cache[i].lru == j)
                {
                    my_cache[i].lru -= 1;
                    break;
                }
            }
        }
        my_cache[lineNum].lru = associativity -1;
    }
    else    /// - if hit but it was not LRU
    {
        temp = my_cache[lineNum].lru;
        my_cache[lineNum].lru = associativity;
        for (i=startLine; i<startLine+associativity; i++)
        {
            for(j = associativity -1; j>-1; j--)
            {
                if( (my_cache[i].lru > temp) && (my_cache[i].lru == j))
                {
                    my_cache[i].lru -= 1;
                    break;
                }
            }
        }
        my_cache[lineNum].lru = associativity -1;
    }
}

/// - Cache initialization
void initCache(struct cacheData *cache, int associativity)
{
    int numberOfLines=cache[0].cacheLines;
    int sizeOfLine=cache[0].cacheLineSize;
    int i,j,k=0;
    for(i=0;i<numberOfLines;i++)
    {
        cache[i].tag=0;
        cache[i].valid=0;
        if ( associativity == FA) /// - Full associative has not set.
        {
            cache[i].set = 0;
        }
        else
        {
            cache[i].set=(int)(i/associativity);
        }
        cache[i].lru=k;
        /// - init LRU count per set.
        if(k == associativity - 1)
        {
            k = 0;
        }
        else
        {
            k++;
        }
        for(j=0;j<sizeOfLine;j++)
        {
            cache[i].data[j]=0;
        }
        
    }
}

/// - Stream buffer initialization
void initStreamBuffer(struct streamBufferData *my_sb)
{
    int i,j;
    for(i=0; i<NUM_OF_STREAM; i++)
    {
        for(j=0; j<MAX_STREAM_BUFFER; j++)
        {
            my_sb[i].addrData[j].tag = 0;
            my_sb[i].addrData[j].setNum = 0;
        }
    }
}

/// - Correlating buffer initialization
void initCorrBuffer(struct patternHistrofyBuffer *my_pht, struct thtData *my_tht)
{
    int i,j;
    for(i = 0; i<MAX_THT_ARRY; i++)
    {
        for(j = 0; j<MAX_THT_ARRY; j++)
        {
            my_tht[i].tag[j] = -1;
            my_pht[i].index[j].tag[0] = -1;
            my_pht[i].index[j].tag[1] = -1;
        }
    }
}

/// - This function finds LRU and return its block number.
int findLru(int associativity,struct cacheData *my_cache, int setNum)
{
    int numberOfLines=my_cache[0].cacheLines;
    int i,lineNum=0;
    
    for(i=0;i<numberOfLines;i++)
    {
        if(associativity == FA)
        {
            if(my_cache[i].tag == 0)
            {
                lineNum = i;
                break;
            }
        }
        else
        {
            if(my_cache[i].set == setNum)
            {
                if(my_cache[i].lru == 0)
                {
                    lineNum = i;
                    break;
                }
            }
        }
    }
    return lineNum; //returning the valid cache line
}

/// - Write cache data
void writeToCache(long blockNum, int offset, int value,struct cacheData *my_cache, int associativity)
{
    int lines=my_cache[0].cacheLines;
    int i=0;
    for(i=0;i<lines;i++)
    {
        if(my_cache[i].tag==blockNum)
        {
            my_cache[i].data[offset]=value;
            break;
        }
    }
}

/// - evic cache block to victim cache
void evicToVictimCache(int tag, struct victimData *victimCache, int sizeOfVictimCache)
{
    int i;
    for(i=0; i<sizeOfVictimCache; i++)
    {
        if(victimCache[i].tag == 0)
        {
            victimCache[i].tag = tag;
            break;
        }
    }
}

/* This function check cache miss. There are 3 different types of miss, Cold, Capacity and conflict.
 Each miss occurs under unique condition so this function checks the conditoin and returns miss type to main function
 */
int checkCacheMiss(int associativity, struct cacheData *my_cache, int setNum, int cacheLine)
{
    int i, startLine = 0, endLine = 0, numOfVacant=0, missType=-1;
    int numberOfLines = my_cache[0].cacheLines;
    
    if ( my_cache[cacheLine].tag == 0)
    {
        missType = COLD_MISS;
        return missType;
    }
    
    if(associativity == FA)
    {
        startLine = associativity * setNum;
        endLine = startLine + numberOfLines;
    }
    else
    {
        startLine = associativity * setNum;
        endLine = startLine + associativity;
    }
    
    for (i = startLine; i < endLine; i++)
    {
        if(my_cache[i].tag == 0)
        {
            numOfVacant++;
            break;
        }
    }
    
    if(numOfVacant == 0)
    {
        if(associativity == FA)
        {
            missType = CAPA_MISS;
        }
        else
        {
            missType = CONF_MISS;
        }
    }
    else
    {
        missType = CAPA_MISS;
    }
    
    return missType;
}

/// - if cache is miss, then evic LRU block then add new block.
int storeIntoCache(long blockNum, int block_size, struct cacheData *my_cache, int associativity, int setNum, struct victimData *victimCache, int sizeOfVictimCache)
{
    int cacheLine, typeOfMiss=-1;
    cacheLine=findLru(associativity,my_cache,setNum);
    typeOfMiss=checkCacheMiss(associativity, my_cache, setNum, cacheLine);
    
    if(my_cache[cacheLine].tag != 0)
    {
        evicToVictimCache(my_cache[cacheLine].tag, victimCache, sizeOfVictimCache);
        my_cache[cacheLine].tag=(int)blockNum;
        my_cache[cacheLine].valid=1;
        if ( associativity != FA)
        {
            reOrderLru(cacheLine, associativity, my_cache, my_cache[cacheLine].set);
        }
    }
    else
    {
        my_cache[cacheLine].tag=(int)blockNum;
        my_cache[cacheLine].valid=1;
        if ( associativity != FA)
        {
            reOrderLru(cacheLine, associativity, my_cache, my_cache[cacheLine].set);
        }
    }
    return typeOfMiss;
}

/* This function checks whether cache block is hit or miss. If its associativity is fully-associative, it searches the tag address from 0 to end and if it is N-way associativity, it only searches specific set from main function.
 */
int checkCacheHit(long blockNum, int associativity, struct cacheData *my_cache, int setNum, int numOfSet)
{
    int i=0, startLine = 0, endLine = 0, numberOfLines = my_cache[0].cacheLines;
    int hit_status=0;
    if ( associativity == FA)
    {
        startLine = associativity * setNum;
        endLine = startLine + numberOfLines;
    }
    else
    {
        startLine = associativity * setNum;
        endLine = startLine + associativity;
    }
    for (i = startLine; i < endLine; i++)
    {
        if(my_cache[i].set == setNum)
        {
            if(my_cache[i].tag==blockNum)
            {
                hit_status=1;
                reOrderLru(i,associativity, my_cache, setNum);
                break;
            }
        }
    }
    if(hit_status==-1)
        hit_status=0;
    return hit_status;
}

/// - This function checks victim cache has the tag address or not. If it has the tag address, then restore the address to cache buffer then erase it from victim cache buffer.
int checkVictimHit(long blockNum, int sizeOfVictim, struct victimData *victimCache, int associativity, struct cacheData *my_cache, int setNum)
{
    int i=0, hit_status = 0, lineNum = 0;
    for(i=0; i<sizeOfVictim; i++)
    {
        if(victimCache[i].tag==blockNum)
        {
            hit_status=1;
            lineNum = findLru(associativity, my_cache, setNum);
            my_cache[lineNum].tag = victimCache[i].tag;
            reOrderLru(lineNum, associativity, my_cache, setNum);
            victimCache[i].tag = 0;
            break;
        }
    }
    return hit_status;
}

/* Description : This function is to search buffer for tag address.
   Input       : Tag address, buffer structure
   Return      : Found(1), Not Found(0)
 */
int checkStreamBuffer(long blockNum, struct streamBufferData *my_sb)
{
    int i, j;
    int bufferHit=0;
    for(i=0; i<NUM_OF_STREAM; i++)
    {
        for(j=0; j<MAX_STREAM_BUFFER; j++)
        {
            if(my_sb[i].addrData[j].tag == blockNum)
            {
                bufferHit = 1;
                my_sb[i].lru = MOST_RECENT_USE;
                return bufferHit;
            }
        }
        
    }
    return bufferHit;
}

/* Description : This function is to write stream/stride buffer by tag address and index number.
   Input       : Tag address, index number, buffer structure, prefetcher type
   Return      : N/A
 */
void writeStreamBuffer(long blockNum, int setNum, struct streamBufferData *my_sb, int pfType)
{
    int i, j, flag = 0;
    
    for (i=0; i<NUM_OF_STREAM; i++)
    {
        if(my_sb[i].lru == 0)
        {
            for(j=0; j<MAX_STREAM_BUFFER; j++)
            {
                if ( pfType == STREAM_BUFFER )
                {
                    my_sb[i].addrData[j].tag = blockNum + j;
                }
                else
                {
                    my_sb[i].addrData[j].tag = blockNum + j*NUM_OF_STRIDE;
                }
                my_sb[i].addrData[j].setNum = setNum;
                my_sb[i].lru = MOST_RECENT_USE;
                flag = 1;
            }
            break;
        }
    }
    if ( flag == 1 )
    {
        for(i = 0; i<NUM_OF_STREAM; i++)
        {
            if(my_sb[i].lru < MOST_RECENT_USE)
            {
                my_sb[i].lru = my_sb[i].lru -1;
            }
        }
    }
}
/* Description : This function is to update THT and PHT using tag address and index number.
   Input       : Tag address, index number, THT structure, PHT structure
   Return      : N/A
 */
void updateHistoryBuffer(long blockNum, int setNum, struct thtData *my_tht, struct patternHistrofyBuffer *my_pht)
{
    int i, flag=0, temp;
    for(i=0; i<MAX_THT_ARRY; i++)
    {
        if(blockNum != my_tht[setNum].tag[i])
        {
            flag++;
        }
    }
    if(flag == MAX_THT_ARRY-1)
    {
        for(i=0; i<MAX_THT_ARRY-1; i++)
        {
            my_tht[setNum].tag[i] = my_tht[setNum].tag[i+1];
        }
        temp = my_tht[setNum].tag[MAX_THT_ARRY-1];
        my_tht[setNum].tag[MAX_THT_ARRY-1] = blockNum;
        for(i=0; i<MAX_PHT_ARRY-1; i++)
        {
            my_pht[setNum].index[i].tag[0] = my_pht[setNum].index[i+1].tag[0];
            my_pht[setNum].index[i].tag[1] = my_pht[setNum].index[i+1].tag[1];
        }
        my_pht[setNum].index[MAX_PHT_ARRY-1].tag[0] = temp;
        my_pht[setNum].index[MAX_PHT_ARRY-1].tag[1] = blockNum;
    }
}
/* Description : This function is to search THT and PHT for tag address by index.
 Input       : Tag address, index number, THT structure, PHT structure
 Return      : Found(1), Not Found(0)
 */
int lookupHistoryBuffer(long blockNum, int setNum, struct thtData *my_tht, struct patternHistrofyBuffer *my_pht)
{
    int i,j, prefetched = -1;
    
    for(i=0; i<MAX_THT_ARRY; i++)
    {
        if(blockNum == my_tht[setNum].tag[i])
        {
            for(j=0; j<MAX_PHT_ARRY; j++)
            {
                if(blockNum == my_pht[setNum].index[j].tag[1])
                {
                    prefetched = 1;
                    break;
                }
            }
        }
    }
    return prefetched;
}

/* This is main function. */
int main (int argc, char *argv[])
{
    int cacheLineSizeOfL1, cacheLineSizeOfL2;
    int numberOfCacheLinesOfL1, numberOfCacheLinesOfL2;
    int associativityOfL1, associtivityOfL2;
    unsigned int i=0, numberOfAccess, sizeOfL1, sizeOfL2, sizeOfL1Victim, sizeOfL2Victim, typeOfPrefetcher;
    struct cacheData *cacheL1, *cacheL2;
    struct victimData *victimCacheOfL1, *victimCacheOfL2;
    struct streamBufferData *streamBufferL1, *streamBufferL2;
    struct patternHistrofyBuffer *correlatePfL1Pht, *correlatePfL2Pht;
    struct thtData *correlatePfL1Tht, *correlatePfL2Tht;
    unsigned int cacheOfL1Hit=0,cacheOfL2Hit=0,victimOfL1Hit=0, victimOfL2Hit=0, coldMissOfL1=0, capaMissOfL1=0, confMissOfL1=0, coldMissOfL2=0, capaMissOfL2=0, confMissOfL2=0,prefetcherHitL1=0, prefetcherHitL2=0;
    long tagAddress=-1;
    int offset, value, flag_hit, flag_victim_hit, flag_pf_hit;
    unsigned int indexL1, indexL2, numberOfSetL1 = 0, numberOfSetL2 = 0;
    unsigned char operation;
    unsigned char memAddress[4];
    unsigned int typeOfMiss;
    long memAddressFourByte;
    char traceFileName[32];
    
    numberOfAccess = atoi(argv[2]);
    typeOfPrefetcher = atoi(argv[3]);
    sizeOfL1 = atoi(argv[4]);
    associativityOfL1 = atoi(argv[5]);
    cacheLineSizeOfL1 = atoi(argv[6]);
    numberOfCacheLinesOfL1 = sizeOfL1 / cacheLineSizeOfL1;
    sizeOfL1Victim = atoi(argv[7]);
    sizeOfL2 = atoi(argv[8]);
    associtivityOfL2 = atoi(argv[9]);
    cacheLineSizeOfL2 = atoi(argv[10]);
    numberOfCacheLinesOfL2 = sizeOfL2 / cacheLineSizeOfL2;
    sizeOfL2Victim = atoi(argv[11]);
    
    FILE *input;
    cacheL1=(struct cacheData *)malloc(numberOfCacheLinesOfL1*sizeof(struct cacheData));
    cacheL2=(struct cacheData *)malloc(numberOfCacheLinesOfL2*sizeof(struct cacheData));
    victimCacheOfL1=(struct victimData *)malloc(sizeOfL1Victim*sizeof(struct victimData));
    victimCacheOfL2=(struct victimData *)malloc(sizeOfL2Victim*sizeof(struct victimData));
    streamBufferL1 = (struct streamBufferData*)malloc(4*sizeof(struct streamBufferData));
    streamBufferL2 = (struct streamBufferData*)malloc(4*sizeof(struct streamBufferData));
    correlatePfL1Tht = (struct thtData*)malloc(4*sizeof(struct thtData));
    correlatePfL2Tht = (struct thtData*)malloc(4*sizeof(struct thtData));
    correlatePfL1Pht = (struct patternHistrofyBuffer*)malloc(4*sizeof(struct patternHistrofyBuffer));
    correlatePfL2Pht = (struct patternHistrofyBuffer*)malloc(4*sizeof(struct patternHistrofyBuffer));
    
    numberOfSetL1 = numberOfCacheLinesOfL1/associativityOfL1;
    numberOfSetL2 = numberOfCacheLinesOfL2/associtivityOfL2;
    
    for(i=0;i<numberOfCacheLinesOfL1;i++)
    {
        cacheL1[i].cacheLineSize=cacheLineSizeOfL1;
        cacheL1[i].cacheLines=numberOfCacheLinesOfL1;
        cacheL1[i].data=(int *)malloc(cacheLineSizeOfL1*sizeof(int));
    }
    
    for(i=0;i<numberOfCacheLinesOfL2;i++)
    {
        cacheL2[i].cacheLineSize=cacheLineSizeOfL2;
        cacheL2[i].cacheLines=numberOfCacheLinesOfL2;
        cacheL2[i].data=(int *)malloc(cacheLineSizeOfL2*sizeof(int));
    }
    
    /*cache initialization*/
    initCache(cacheL1,associativityOfL1);
    initCache(cacheL2,associtivityOfL2);
    initStreamBuffer(streamBufferL1);
    initStreamBuffer(streamBufferL2);
    initCorrBuffer(correlatePfL1Pht, correlatePfL1Tht);
    initCorrBuffer(correlatePfL2Pht, correlatePfL2Tht);
    
    
    sprintf(traceFileName, "%s.trace", argv[1]);
    
    input = fopen(traceFileName, "r");
    
    if(input)
    {
        printf("Opened file %s for reading\n", traceFileName);
    }
    else
    {
        printf("File does not exist\n");
        exit(0);
    }
    
    /// - Iteration until number of accesses from the input.
    for( i = 0; i < numberOfAccess; i++)
    {
        /// - Read address and operation from trace file.
        memAddressFourByte = 0;
        fread(memAddress, 4, 1, input);
        memAddressFourByte = memAddress[0];
        memAddressFourByte = memAddressFourByte << 24;
        memAddressFourByte = memAddressFourByte | (memAddress[1] << 16) | (memAddress[2] << 8) | (memAddress[3] << 0);
        fread(&operation, sizeof(unsigned char), 1, input);
        if(operation=='r')
        {
            int bitShiftForAddress, indexBit, offsetBit;
            long tagAddressBit;

            /// - Find index bits and tag bits from the address.
            offsetBit = ((cacheLineSizeOfL1 - 1));
            tagAddressBit = (0xFFFFFF - offsetBit) << 2;
            offset = (memAddressFourByte & offsetBit) + 2;
            offsetBit = ((int) log2(cacheLineSizeOfL1));
            indexBit = numberOfSetL1 - 1;
            bitShiftForAddress = indexBit << (offsetBit);
            indexBit = (int) log2(numberOfSetL1);
            tagAddressBit = tagAddressBit - bitShiftForAddress;
            tagAddress = (memAddressFourByte & tagAddressBit) >> (offsetBit + indexBit);/// - This is a tag address
            indexL1 = (memAddressFourByte & bitShiftForAddress) >> (offsetBit); /// - This is a set number

            
            /// - Check cache block is hit or not then count it up if it is hit.
            flag_hit=checkCacheHit(tagAddress,associativityOfL1, cacheL1, indexL1, numberOfSetL1);
            if(flag_hit==1)
            {
                cacheOfL1Hit++;
                /// - If cache hit, determine it is already prefetched tag address or not. It it is, increment prefetcherHit by 1.
                switch (typeOfPrefetcher) {
                    case STREAM_BUFFER:
                    case STRIDE_PREFETCHER:
                    {
                        flag_pf_hit = checkStreamBuffer(tagAddress, streamBufferL1);
                        if(flag_pf_hit == 1)
                        {
                            prefetcherHitL1++;
                        }
                        break;
                    }
                    case CORRELATING_PREFETCHER:
                    {
                        flag_pf_hit = lookupHistoryBuffer(tagAddress, indexL1, correlatePfL1Tht, correlatePfL1Pht);
                        if(flag_pf_hit == 1)
                        {
                            prefetcherHitL1++;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            else
            {
                /// - If it is not hit, search victim cache buffer once again.
                flag_victim_hit = checkVictimHit(tagAddress, sizeOfL1Victim, victimCacheOfL1, associativityOfL1, cacheL1, indexL1);
                if(flag_victim_hit == 1)
                {
                    victimOfL1Hit++;
                }
                else
                {
                    /// - Write to buffer by types of prefetchers.
                    switch (typeOfPrefetcher) {
                        case STREAM_BUFFER:
                        {
                            writeStreamBuffer(tagAddress+4, indexL1, streamBufferL1, STREAM_BUFFER);
                            break;
                        }
                        case STRIDE_PREFETCHER:
                        {
                            writeStreamBuffer(tagAddress+NUM_OF_STRIDE, indexL1, streamBufferL1, STRIDE_PREFETCHER);
                            break;
                        }
                        case CORRELATING_PREFETCHER:
                        {
                            updateHistoryBuffer(tagAddress, indexL1, correlatePfL1Tht, correlatePfL1Pht);
                            break;
                        }
                        default:
                            break;
                    }
                    
                    /// - If the cache block is found in nowhere, we need to find LRU, evic it then add new address.
                    typeOfMiss = storeIntoCache(tagAddress, cacheLineSizeOfL1, cacheL1, associativityOfL1, indexL1, victimCacheOfL1, sizeOfL1Victim);
                    switch (typeOfMiss)
                    {
                        case COLD_MISS:
                            coldMissOfL1++;
                            break;
                        case CONF_MISS:
                            confMissOfL1++;
                            break;
                        case CAPA_MISS:
                            capaMissOfL1++;
                            break;
                        default:
                            break;
                    }
                }
            }
            /// - L2 cache
            offsetBit = cacheLineSizeOfL2 - 1;
            tagAddressBit = (0xFFFFFF - offsetBit) << 2;
            offset = (memAddressFourByte & offsetBit);
            offsetBit = (int) log2(cacheLineSizeOfL2) + 2;
            indexBit = numberOfSetL2 - 1;
            bitShiftForAddress = indexBit << (offsetBit);
            indexBit = (int) log2(numberOfSetL2);
            tagAddressBit = tagAddressBit - bitShiftForAddress;
            tagAddress = (memAddressFourByte & tagAddressBit) >> (offsetBit + indexBit);
            indexL2 = (memAddressFourByte & bitShiftForAddress) >> (offsetBit);
            flag_hit = 0;
            flag_victim_hit = 0;
            flag_pf_hit = 0;
            
            flag_hit=checkCacheHit(tagAddress,associtivityOfL2, cacheL2, indexL2, numberOfSetL2);
            if(flag_hit==1)
            {
                cacheOfL2Hit++;
                /// - If cache hit, determine it is already prefetched tag address or not. It it is, increment prefetcherHit by 1.
                switch (typeOfPrefetcher) {
                    case STREAM_BUFFER:
                    case STRIDE_PREFETCHER:
                    {
                        flag_pf_hit = checkStreamBuffer(tagAddress, streamBufferL2);
                        if(flag_pf_hit == 1)
                        {
                            prefetcherHitL2++;
                        }
                        break;
                    }
                    case CORRELATING_PREFETCHER:
                    {
                        flag_pf_hit = lookupHistoryBuffer(tagAddress, indexL2, correlatePfL2Tht, correlatePfL2Pht);
                        if(flag_pf_hit == 1)
                        {
                            prefetcherHitL2++;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            else
            {
                flag_victim_hit = checkVictimHit(tagAddress, sizeOfL2Victim, victimCacheOfL2, associtivityOfL2, cacheL2, indexL2);
                if(flag_victim_hit == 1)
                {
                    victimOfL2Hit++;
                }
                else
                {
                    /// - Write to buffer by types of prefetchers.
                    switch (typeOfPrefetcher) {
                        case STREAM_BUFFER:
                        {
                            writeStreamBuffer(tagAddress+1, indexL1, streamBufferL2, STREAM_BUFFER);
                            break;
                        }
                        case STRIDE_PREFETCHER:
                        {
                            writeStreamBuffer(tagAddress+NUM_OF_STRIDE, indexL1, streamBufferL2, STRIDE_PREFETCHER);
                            break;
                        }
                        case CORRELATING_PREFETCHER:
                        {
                            updateHistoryBuffer(tagAddress, indexL2, correlatePfL2Tht, correlatePfL2Pht);
                            break;
                        }
                        default:
                            break;
                    }
                    typeOfMiss=storeIntoCache(tagAddress, cacheLineSizeOfL2, cacheL2, associtivityOfL2, indexL2, victimCacheOfL2, sizeOfL2Victim);
                    switch (typeOfMiss)
                    {
                        case COLD_MISS:
                            coldMissOfL2++;
                            break;
                        case CONF_MISS:
                            confMissOfL2++;
                            break;
                        case CAPA_MISS:
                            capaMissOfL2++;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        /// - In case of operation is write.
        else if(operation=='w')
        {
            int bitShiftForAddress, indexBit, offsetBit;
            long tagAddressBit;
            
            offsetBit = ((cacheLineSizeOfL1 - 1));
            tagAddressBit = (0xFFFFFF - offsetBit) << 2;
            offset = (memAddressFourByte & offsetBit) + 2;
            offsetBit = ((int) log2(cacheLineSizeOfL1));
            indexBit = numberOfSetL1 - 1;
            bitShiftForAddress = indexBit << (offsetBit);
            indexBit = (int) log2(numberOfSetL1);
            tagAddressBit = tagAddressBit - bitShiftForAddress;
            tagAddress = (memAddressFourByte & tagAddressBit) >> (offsetBit + indexBit);/// - This is a tag address
            indexL1 = (memAddressFourByte & bitShiftForAddress) >> (offsetBit); /// - This is a set number
            
            flag_hit=checkCacheHit(tagAddress,associativityOfL1, cacheL1, indexL1, numberOfSetL1);
            if(flag_hit==1)
            {
                cacheOfL1Hit++;
                //   writeToCache(tagAddress, offset, value, cacheL1, associativityOfL1);
            }
            else
            {
                /// - If it is not hit, search victim cache buffer once again.
                flag_victim_hit = checkVictimHit(tagAddress, sizeOfL1Victim, victimCacheOfL1, associativityOfL1, cacheL1, indexL1);
                if(flag_victim_hit == 1)
                {
                    victimOfL1Hit++;
                }
                else
                {
                    /// - If the cache block is found in nowhere, we need to find LRU, evic it then add new address.
                    typeOfMiss = storeIntoCache(tagAddress, cacheLineSizeOfL1, cacheL1, associativityOfL1, indexL1, victimCacheOfL1, sizeOfL1Victim);
                    switch (typeOfMiss)
                    {
                        case COLD_MISS:
                            coldMissOfL1++;
                            break;
                        case CONF_MISS:
                            confMissOfL1++;
                            break;
                        case CAPA_MISS:
                            capaMissOfL1++;
                            break;
                        default:
                            break;
                    }
                }
            }
            /// - L2 Cache
            offsetBit = cacheLineSizeOfL2 - 1;
            tagAddressBit = (0xFFFFFF - offsetBit) << 2;
            offset = (memAddressFourByte & offsetBit);
            offsetBit = (int) log2(cacheLineSizeOfL2) + 2;
            indexBit = numberOfSetL2 - 1;
            bitShiftForAddress = indexBit << (offsetBit);
            indexBit = (int) log2(numberOfSetL2);
            tagAddressBit = tagAddressBit - bitShiftForAddress;
            tagAddress = (memAddressFourByte & tagAddressBit) >> (offsetBit + indexBit);
            indexL2 = (memAddressFourByte & bitShiftForAddress) >> (offsetBit);
            
            flag_hit=checkCacheHit(tagAddress,associtivityOfL2, cacheL2, indexL2, numberOfSetL2);
            if(flag_hit==1)
            {
                cacheOfL2Hit++;
                writeToCache(tagAddress, offset, value, cacheL2, associtivityOfL2);
            }
        }
        else
        {
            break;
        }
    }
    char pfType[6];
    if(typeOfPrefetcher == STREAM_BUFFER)
    {
        strncpy(pfType, "STREAM", 6);
    }
    else if(typeOfPrefetcher == STRIDE_PREFETCHER)
    {
        strncpy(pfType, "STRIDE", 6);
    }
    else
    {
        strncpy(pfType, "CORREL", 6);
    }
    printf("---------------------------------------------------\n");
    printf("L1 Cache Status =>\nL1 Hit\t\t\t : %u\nL1 Victim Hit\t\t : %u\nL1 Cold Miss\t\t : %u\nL1 Capa Miss\t\t : %u\nL1 Conf Miss\t\t : %u\nL1 PF Hit(%s)\t : %u\n",cacheOfL1Hit, victimOfL1Hit, coldMissOfL1, capaMissOfL1, confMissOfL1, pfType, prefetcherHitL1);
    printf("---------------------------------------------------\n");
    printf("L2 Cache Status =>\nL2 Hit\t\t\t : %u\nL2 Victim Hit\t\t : %u\nL2 Cold Miss\t\t : %u\nL2 Capa Miss\t\t : %u\nL2 Conf Miss\t\t : %u\nL2 PF Hit(%s)\t : %u\n",cacheOfL2Hit, victimOfL2Hit, coldMissOfL2, capaMissOfL2, confMissOfL2, pfType, prefetcherHitL2);
    printf("---------------------------------------------------\n");
}
