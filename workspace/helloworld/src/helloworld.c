#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

int main(void)
{
    printf("\nHello, world!\n\n");
    int fd;
    char *pPCM=mmap(0x10002000, 0x800,PROT_READ | PROT_WRITE,MAP_FIXED, fd,0 );
    if (pPCM == MAP_FAILED) 
    {
        printf("map failed.\r\n");
        return -1;
    }
    printf("mmap ok\r\n");
    int32_t *ptest = (int32_t *)pPCM;
    printf("addr:0x%08x\r\n",ptest);
    for(int i=0;i<512;i++)
    {
        printf("0x%08x ",*ptest+i);
        if(i%8 == 7)
        {
            printf("\r\n");
        }
    }
    return 0;
}
