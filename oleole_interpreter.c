/**
 *  gcc -g -Wall oleole_interpreter.c -o oleole_interpreter
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/ioctl.h>
#include <linux/types.h>

#define OLEOLE_IOC_MAGIC 'o'

#define OLEOLE_IOC_START		_IOW(OLEOLE_IOC_MAGIC, 1, __u64)
#define OLEOLE_IOC_SETCR3		_IOW(OLEOLE_IOC_MAGIC, 2, __u32)

const uint64_t SIZE512GB = 0x8000000000UL;


enum {
    GUEST_PHY_MEM_SIZE = 16 * 1024 * 1024
};


static void set_cr3(int fd, uint32_t cr3);


int main(int argc, char** argv)
{
    int ret, fd;

    fd = open("/proc/oleolevm", O_RDWR);

    /*
     *  mmap 前にゲストの物理メモリを設定
     */
    ret = ioctl(fd, OLEOLE_IOC_START, GUEST_PHY_MEM_SIZE);
    if (ret < 0) {
        int eno = errno;
        fprintf(stderr, "ioctl(fd, OLEOLE_IOC_START, ) errno = %d", eno);
        exit(EXIT_FAILURE);
    }

    /*
     *  /proc/oleolevm は 512GB 境界に沿う 512GB を mmap する。
     */
    void *p = mmap((void*)(uintptr_t)SIZE512GB, SIZE512GB, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
    if (p == MAP_FAILED) {
        int eno = errno;
        fprintf(stderr, "mmap() errno = %d", eno);
        exit(EXIT_FAILURE);
    }

    /*
     *  mmap した領域の先頭から 4GB はゲスト物理、8GB から 4GB はゲスト仮想 
     *
     *   [0x000000000, 0x100000000) ゲスト物理
     *   [0x200000000, 0x300000000) ゲスト仮想
     *
     *  ゲスト仮想領域は CR3 が指定する位置にセグメントテーブルから検索を行う。
     *  
     *  仮想アドレスは
     *    [31:22] セグメントインデックス
     *    [21:12] ページインデックス
     *    [11: 0] ページ内オフセット
     *
     *  Segment-Table-Entry(STE) も Page-Table-Entry(PTE) も上位 20 ビット
     *  下位1ビット([0])が present bit で、次の1ビット([1])が wriet protection bit。
     */
    uint8_t *paddr = p;
    uint8_t *vaddr = paddr + 0x200000000UL;

    /* S1: CR3 を 0x000 に指定 */
    set_cr3(fd, 0);

    /* S2: 仮想ページの 0x0000 が物理ページの 0x2000 にマップする */
    *(uint32_t*)(paddr +      0) = 0x1001;
    *(uint32_t*)(paddr + 0x1000) = 0x2001;

    /* S3: 仮想ページの 0x0000 に書き込んで見る。 */
    *(uint32_t*)(vaddr         ) = 0xCAFEBABE;

    /* S4: 書き込んだ内容が物理ページから読み出せることを確認。 */
    printf("PMEM: 0x2000 = %08x\n", *(uint32_t*)(paddr + 0x2000));

    /* S5: 仮想ページの 0x0000 を物理ページの 0x2000 に変更 */
    *(uint32_t*)(paddr +      0) = 0x1001;
    *(uint32_t*)(paddr + 0x1000) = 0x3001;

    /* S6: 仮想ページの 0x0000 に書き込んで見る。 */
    *(uint32_t*)(vaddr         ) = 0xDEADBEAF;

    /* S7: しかし S6 の内容は物理ページ 0x3000 ではなく元の 0x2000 に書き込まれる */
    printf("PMEM: 0x2000 = %08x\n", *(uint32_t*)(paddr + 0x2000));
    printf("PMEM: 0x3000 = %08x\n", *(uint32_t*)(paddr + 0x3000));

    /* S8: CR3 に 0x000 を再設定 */
    set_cr3(fd, 0);

    /* S9: S6 と同じ内容を書き込んでみる。 */
    *(uint32_t*)(vaddr         ) = 0xDEADBEAF;

    /* S10: 今度は 0x3000 に書き込まれた。 */
    printf("PMEM: 0x2000 = %08x\n", *(uint32_t*)(paddr + 0x2000));
    printf("PMEM: 0x3000 = %08x\n", *(uint32_t*)(paddr + 0x3000));

    close(fd);

    return 0;
}


static void set_cr3(int fd, uint32_t cr3)
{
    int ret;
    printf("ioctl(fd, OLEOLE_IOC_SETCR3, %x)\n", cr3);
    ret = ioctl(fd, OLEOLE_IOC_SETCR3, cr3);
    if (ret < 0) {
        int eno = errno;
        fprintf(stderr, "ioctl(fd, OLEOLE_IOC_SETCR3, ) errno = %d", eno);
        exit(EXIT_FAILURE);
    }
}
