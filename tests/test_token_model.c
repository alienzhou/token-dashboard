#include "token_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t packet[TOKEN_PACKET_SIZE];
static void crc(void)
{
    uint32_t c=token_crc32(packet,sizeof(packet)-4);
    for (unsigned i=0;i<4;++i) packet[sizeof(packet)-4+i]=c>>(8*i);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb"); assert(f);
    assert(fread(packet,1,sizeof(packet),f)==sizeof(packet)); fclose(f);
    token_snapshot_t s;
    assert(token_decode(packet,sizeof(packet),&s));
    assert(s.sequence==123);
    assert(s.sources[0].total==123456789012ULL);
    assert(token_total(&s,-1)==123456789012ULL);
    assert(token_day(&s,-1,363)==54321);
    assert(token_day(&s,-1,364)==0);
    assert(token_heat_level(UINT64_MAX-1,UINT64_MAX)==4);
    assert(token_heat_level(0,0)==0);
    char value[32]; token_format(UINT64_MAX,value,sizeof(value)); assert(strlen(value)<12);
    packet[3]^=1; assert(!token_decode(packet,sizeof(packet),&s)); packet[3]^=1;
    packet[150]^=1; assert(!token_decode(packet,sizeof(packet),&s)); packet[150]^=1;
    assert(!token_decode(packet,sizeof(packet)-1,&s));
    uint8_t saved=packet[52]; packet[52]=7; crc(); assert(!token_decode(packet,sizeof(packet),&s)); packet[52]=saved; crc();
    for (unsigned payload=14;payload<200;payload+=17) {
        token_receiver_t rx={0};
        for (size_t off=0;off<sizeof(packet);off+=payload) {
            uint8_t chunk[206]={123,0,0,0,(uint8_t)off,(uint8_t)(off>>8)};
            size_t n=sizeof(packet)-off; if(n>payload)n=payload;
            memcpy(chunk+6,packet+off,n);
            assert(token_receive(&rx,chunk,n+6)==(off+n==sizeof(packet)?1:0));
            if(off+n<sizeof(packet))assert(token_receive(&rx,chunk,n+6)==0);
        }
        assert(!memcmp(rx.bytes,packet,sizeof(packet)));
    }
    token_receiver_t rx={0}; uint8_t chunk[20]={123,0,0,0,1};
    assert(token_receive(&rx,chunk,sizeof(chunk))==-1);
    chunk[4]=0; assert(token_receive(&rx,chunk,sizeof(chunk))==0);
    chunk[0]=124;chunk[4]=14;assert(token_receive(&rx,chunk,sizeof(chunk))==-1);
    chunk[0]=123;chunk[4]=0;chunk[6]=1;assert(token_receive(&rx,chunk,sizeof(chunk))==-1);
    puts("Token protocol: PASS (Python fixture, CRC, validation, duplicate/out-of-order chunks, uint64)");
    return 0;
}
