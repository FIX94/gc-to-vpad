#include "loader.h"

#define RW_MEM_MAP 0xA0000000
#if (VER==310)
#define GC_READER_ADDR 0x011D2000
#define PAD_LOC_ADDR 0x011D2FFC
#define VPAD_PATCH_ADDR 0x011D3000
#define MAIN_JMP_ADDR 0x0101894C
#include "vpad_patch310.h"
#include "gc_reader310.h"
#else //if(VER==532)
#define GC_READER_ADDR 0x011DD000
#define PAD_LOC_ADDR 0x011DDFFC
#define VPAD_PATCH_ADDR 0x011DE000
#define MAIN_JMP_ADDR 0x0101C55C
#include "vpad_patch532.h"
#include "gc_reader532.h"
#endif

#define assert(x) \
    do { \
        if (!(x)) \
            OSFatal("Assertion failed " #x ".\n"); \
    } while (0)

#define ALIGN_BACKWARD(x,align) \
	((typeof(x))(((unsigned int)(x)) & (~(align-1))))

int doBL( unsigned int dst, unsigned int src );

void _start()
{
    /* Load a good stack */
    asm(
        "lis %r1, 0x1ab5 ;"
        "ori %r1, %r1, 0xd138 ;"
    );

    /* Get a handle to coreinit.rpl. */
    unsigned int coreinit_handle;
    OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);

	/* Get for later hook */
    unsigned int nsyshid_handle;
    OSDynLoad_Acquire("nsyshid.rpl", &nsyshid_handle);

    unsigned int vpad_handle;
    OSDynLoad_Acquire("vpad.rpl", &vpad_handle);

    /* Load a few useful symbols. */
	void (*memcpy)(void *dst, const void *src, int bytes);
	void (*memset)(void *dst, char val, int bytes);
    void (*_Exit)(void) __attribute__ ((noreturn));
    void (*DCFlushRange)(const void *, int);
	void (*ICInvalidateRange)(const void *, int);
    void *(*OSEffectiveToPhysical)(const void *);
	void*(*OSAllocFromSystem)(uint32_t size, int align);
	void (*OSFreeToSystem)(void *ptr);

    OSDynLoad_FindExport(coreinit_handle, 0, "_Exit", &_Exit);
	OSDynLoad_FindExport(coreinit_handle, 0, "memcpy", &memcpy);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
    OSDynLoad_FindExport(coreinit_handle, 0, "DCFlushRange", &DCFlushRange);
	OSDynLoad_FindExport(coreinit_handle, 0, "ICInvalidateRange", &ICInvalidateRange);
    OSDynLoad_FindExport(coreinit_handle, 0, "OSEffectiveToPhysical", &OSEffectiveToPhysical);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);

    assert(_Exit);
    assert(memcpy);
    assert(DCFlushRange);
	assert(ICInvalidateRange);
	assert(OSEffectiveToPhysical);
	assert(OSAllocFromSystem);
	assert(OSFreeToSystem);

	//IM functions
	int(*IM_SetDeviceState)(int fd, void *mem, int state, int a, int b);
	int(*IM_Close)(int fd);
	int(*IM_Open)();

	OSDynLoad_FindExport(coreinit_handle, 0, "IM_SetDeviceState", &IM_SetDeviceState);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Close", &IM_Close);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Open", &IM_Open);

	assert(IM_SetDeviceState);
	assert(IM_Close);
	assert(IM_Open);

	//Restart system to get lib access
	int fd = IM_Open();
	void *mem = OSAllocFromSystem(0x100, 64);
	memset(mem, 0, 0x100);
	//set restart flag to force quit browser
	IM_SetDeviceState(fd, mem, 3, 0, 0); 
	IM_Close(fd);
	OSFreeToSystem(mem);
	//wait a bit for browser end
	unsigned int t1 = 0x1FFFFFFF;
	while(t1--) ;

    /* Make sure the kernel exploit has been run */
	if( OSEffectiveToPhysical((void *)RW_MEM_MAP) != (void *)0x10000000 &&
        OSEffectiveToPhysical((void *)RW_MEM_MAP) != (void *)0x30000000 &&
	    OSEffectiveToPhysical((void *)RW_MEM_MAP) != (void *)0x31000000 )
	{
        OSFatal("You must run ksploit before installing GC to VPAD.");
    }
	else
	{
		/* Get the socket function to patch */
		unsigned int *readmove = 0;
		OSDynLoad_FindExport(vpad_handle, 0, "VPADInit", &readmove);
		if(!(((unsigned int)readmove) & 0x01000000)) OSFatal("VPADInit not found!");
		readmove -= (0x24/4); //VPADRead: mr r3, r31; our hook
		if(*readmove != 0x7FE3FB78) OSFatal("Something with the VPAD RPL is different!");
		unsigned int topatch = (unsigned int)readmove;

		/* Our main writable area */
		unsigned int physWriteLoc = (unsigned int)OSEffectiveToPhysical((void*)RW_MEM_MAP);

		/* Install gc reader */
		unsigned int phys_gc_reader_loc = (unsigned int)OSEffectiveToPhysical((void*)GC_READER_ADDR);
		void *gc_reader_loc = (unsigned int*)(RW_MEM_MAP + (phys_gc_reader_loc - physWriteLoc));

		memcpy(gc_reader_loc, gc_reader_text_bin, gc_reader_text_bin_len);
		DCFlushRange(gc_reader_loc, gc_reader_text_bin_len);
		ICInvalidateRange(gc_reader_loc, gc_reader_text_bin_len);

		/* Patch start main */
		unsigned int phys_main_jmp_loc = (unsigned int)OSEffectiveToPhysical((void*)MAIN_JMP_ADDR);
		void *main_jmp_loc = (unsigned int*)(RW_MEM_MAP + (phys_main_jmp_loc - physWriteLoc));

		*((uint32_t *)main_jmp_loc) = doBL(GC_READER_ADDR, MAIN_JMP_ADDR);
		DCFlushRange(ALIGN_BACKWARD(main_jmp_loc, 32), 0x20);
		ICInvalidateRange(ALIGN_BACKWARD(main_jmp_loc, 32), 0x20);

		/* Make sure to start with no location yet */
		unsigned int physPadLoc = (unsigned int)OSEffectiveToPhysical((void*)PAD_LOC_ADDR);
		unsigned int *PadMemLocW = (unsigned int*)(RW_MEM_MAP + (physPadLoc - physWriteLoc));
		*PadMemLocW = 0; //IMPORTANT

        /* Install vpad patch */
		unsigned int phys_vpad_patch_loc = (unsigned int)OSEffectiveToPhysical((void*)VPAD_PATCH_ADDR);
		void *vpad_patch_loc = (unsigned int*)(RW_MEM_MAP + (phys_vpad_patch_loc - physWriteLoc));
		memcpy(vpad_patch_loc, vpad_patch_text_bin, vpad_patch_text_bin_len);
		DCFlushRange(vpad_patch_loc, vpad_patch_text_bin_len);
		ICInvalidateRange(vpad_patch_loc, vpad_patch_text_bin_len);

		/* Patch VPADRead */
		unsigned int phys_topatch_loc = (unsigned int)OSEffectiveToPhysical((void*)topatch);
		unsigned int *topatch_loc = (unsigned int*)(RW_MEM_MAP + (phys_topatch_loc - physWriteLoc));
		*topatch_loc = doBL(VPAD_PATCH_ADDR, topatch);
		DCFlushRange(ALIGN_BACKWARD(topatch_loc, 32), 0x20);
		ICInvalidateRange(ALIGN_BACKWARD(topatch_loc, 32), 0x20);

        /* The fix for Splatoon and such */
		//broken with this atm because we still use it
		//kern_write((void*)(KERN_ADDRESS_TBL + (0x12 * 4)), 0x00000000);
		//kern_write((void*)(KERN_ADDRESS_TBL + (0x13 * 4)), 0x14000000);     
	}

    _Exit();
}

int doBL( unsigned int dst, unsigned int src )
{
	unsigned int newval = (dst - src);
	newval&= 0x03FFFFFC;
	newval|= 0x48000001;
	return newval;
}

/* Write a 32-bit word with kernel permissions */
void kern_write(void *addr, uint32_t value)
{
	asm(
		"li 3,1\n"
		"li 4,0\n"
		"mr 5,%1\n"
		"li 6,0\n"
		"li 7,0\n"
		"lis 8,1\n"
		"mr 9,%0\n"
		"mr %1,1\n"
		"li 0,0x3500\n"
		"sc\n"
		"nop\n"
		"mr 1,%1\n"
		:
		:	"r"(addr), "r"(value)
		:	"memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
			"11", "12"
		);
}
