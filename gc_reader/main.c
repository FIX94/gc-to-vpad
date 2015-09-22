#include "main.h"

typedef struct
{
	unsigned int handle;
	unsigned int physical_device_inst;
	unsigned short vid;
	unsigned short pid;
	unsigned char interface_index;
	unsigned char sub_class;
	unsigned char protocol;

	unsigned short max_packet_size_rx;
	unsigned short max_packet_size_tx;

} HIDDevice;

typedef struct _HIDClient HIDClient;

#define HID_DEVICE_DETACH	0
#define HID_DEVICE_ATTACH	1

typedef int (*HIDAttachCallback)(HIDClient *p_hc,HIDDevice *p_hd,unsigned int attach);

struct _HIDClient
{
	HIDClient *next;
	HIDAttachCallback attach_cb;
};

typedef void (*HIDCallback)(unsigned int handle,int error,unsigned char *p_buffer,unsigned int bytes_transferred,void *p_user);

#define DEVICE_UNUSED 0
#define DEVICE_USED 1

static void my_read_cb(unsigned int handle, int error, unsigned char *buf, unsigned int bytes_transfered, void *p_user);
static void my_write_cb(unsigned int handle, int error, unsigned char *buf, unsigned int bytes_transfered, void *p_user);
static int my_attach_cb(HIDClient *p_client, HIDDevice *p_device, unsigned int attach);

int _start(int argc, char *argv[]) {
	//get coreinit_handle
	unsigned int tmpstr[8];
	unsigned int coreinit_handle;
	tmpstr[0] = 0x636F7265; tmpstr[1] = 0x696E6974; tmpstr[2] = 0; 
	OSDynLoad_Acquire((char*)tmpstr, &coreinit_handle);

	//get OSAllocFromSystem and get 12 bytes for pad
	void*(*OSAllocFromSystem)(unsigned int size, int align);
	tmpstr[0] = 0x4F53416C; tmpstr[1] = 0x6C6F6346; tmpstr[2] = 0x726F6D53; tmpstr[3] = 0x79737465; tmpstr[4] = 0x6D000000;   
	OSDynLoad_FindExport(coreinit_handle,0,(char*)tmpstr,&OSAllocFromSystem);

	unsigned int *padreadmem = OSAllocFromSystem(12,4); //safe mem
	padreadmem[0] = 0; padreadmem[1] = 0; padreadmem[2] = 0;
	*(unsigned int*)(&PadMemLocW) = (unsigned int)padreadmem; //UGLY 

	//get OSFreeToSystem for later usage
	void(*OSFreeToSystem)(void *ptr);
	tmpstr[0] = 0x4F534672; tmpstr[1] = 0x6565546F; tmpstr[2] = 0x53797374; tmpstr[3] = 0x656D0000;   
	OSDynLoad_FindExport(coreinit_handle,0,(char*)tmpstr,&OSFreeToSystem);

	//get nsyshid handle
	unsigned int nsyshid_handle;
	tmpstr[0] = 0x6E737973; tmpstr[1] = 0x68696400;   
	OSDynLoad_Acquire((char*)tmpstr, &nsyshid_handle);

	//get HIDAddClient and HIDDelClient
	int(*HIDAddClient)(HIDClient *p_client, HIDAttachCallback attach_callback);
	tmpstr[0] = 0x48494441; tmpstr[1] = 0x6464436C; tmpstr[2] = 0x69656E74; tmpstr[3] = 0; 
	OSDynLoad_FindExport(nsyshid_handle, 0, (char*)tmpstr, &HIDAddClient);

	int(*HIDDelClient)(HIDClient *p_client);
	tmpstr[0] = 0x48494444; tmpstr[1] = 0x656C436C; tmpstr[2] = 0x69656E74; tmpstr[3] = 0; 
	OSDynLoad_FindExport(nsyshid_handle, 0, (char*)tmpstr, &HIDDelClient);

	//start HID reads for game
	HIDClient *fd = memalign(sizeof(HIDClient), 0x40);
	HIDAddClient(fd, my_attach_cb);

	//run game, reads keep going
	int ret = main(argc, argv);

	//stop HID reads before quit
	*(unsigned int*)(&PadMemLocW) = 0;
	OSFreeToSystem(padreadmem);

	HIDDelClient(fd);
	free(fd);

	return ret;
}

struct my_cb_user {
	unsigned char *buf;
	unsigned int nsyshid_handle;
	void *HIDRead;
	void *HIDWrite;
	void *VPADBASEGetMotorOnRemainingCount;
	int rumblestatus;
	unsigned int transfersize;
};

static void my_read_cb(unsigned int handle, int error, unsigned char *buf, unsigned int bytes_transfered, void *p_user)
{
	if(error == 0)
	{
		unsigned int *src = (unsigned int*)buf;
		unsigned int *dst = (unsigned int*)(*(unsigned int*)(&PadMemLoc));
		//copy inputted data into our mem and continue reading
		dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
		struct my_cb_user *usr = (struct my_cb_user*)p_user;
		int(*VPADBASEGetMotorOnRemainingCount)(int drc) = usr->VPADBASEGetMotorOnRemainingCount;
		int currumblestatus = !!VPADBASEGetMotorOnRemainingCount(0);
		if(currumblestatus != usr->rumblestatus)
		{
			usr->rumblestatus = currumblestatus;
			usr->buf[0] = 0x11; usr->buf[1] = usr->rumblestatus; usr->buf[2] = 0; usr->buf[3] = 0; usr->buf[4] = 0;
			int(*HIDWrite)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user) = usr->HIDWrite;
			HIDWrite(handle, usr->buf, 5, my_write_cb, usr);
		}
		else
		{
			int(*HIDRead)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user) = usr->HIDRead;
			HIDRead(handle, usr->buf, bytes_transfered, my_read_cb, usr);
		}
	}
}

static void my_write_cb(unsigned int handle, int error, unsigned char *buf, unsigned int bytes_transfered, void *p_user)
{
	if(error == 0)
	{
		//controller started up, lets start read chain up
		struct my_cb_user *usr = (struct my_cb_user*)p_user;
		int(*HIDRead)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user) = usr->HIDRead;
		HIDRead(handle, usr->buf, usr->transfersize, my_read_cb, usr);
	}
}

#define SWAP16(x) ((x>>8) | ((x&0xFF)<<8))
static int my_attach_cb(HIDClient *p_client, HIDDevice *p_device, unsigned int attach)
{
	if( attach && SWAP16(p_device->vid) == 0x057e && SWAP16(p_device->pid) == 0x0337 )
	{
		//adapter init value
		unsigned char *buf = memalign(64,64);
		memset(buf,0,64);
		buf[0] = 0x13;
		//user struct for vars
		struct my_cb_user *usr = memalign(64,64);
		usr->transfersize = p_device->max_packet_size_rx;
		usr->buf = buf;
		//get nsyshid handle
		unsigned int nsyshid_handle;
		unsigned int tmpstr[12];
		tmpstr[0] = 0x6E737973; tmpstr[1] = 0x68696400;
		OSDynLoad_Acquire((char*)tmpstr, &nsyshid_handle);
		usr->nsyshid_handle = nsyshid_handle;
		//set up HIDRead and HIDWrite
		tmpstr[0] = 0x48494452; tmpstr[1] = 0x65616400;
		OSDynLoad_FindExport(usr->nsyshid_handle,0,(char*)tmpstr,&usr->HIDRead);
		tmpstr[0] = 0x48494457; tmpstr[1] = 0x72697465; tmpstr[2] = 0;
		OSDynLoad_FindExport(usr->nsyshid_handle,0,(char*)tmpstr,&usr->HIDWrite);
		//set up rumble
		usr->rumblestatus = 0;
		unsigned int vpadbase_handle;
		tmpstr[0] = 0x76706164; tmpstr[1] = 0x62617365; tmpstr[2] = 0;
		OSDynLoad_Acquire((char*)tmpstr, &vpadbase_handle);
		tmpstr[0] = 0x56504144; tmpstr[1] = 0x42415345; tmpstr[2] = 0x4765744D; tmpstr[3] = 0x6F746F72; tmpstr[4] = 0x4F6E5265;
		tmpstr[5] = 0x6D61696E; tmpstr[6] = 0x696E6743; tmpstr[7] = 0x6F756E74; tmpstr[8] = 0;
		OSDynLoad_FindExport(vpadbase_handle,0,(char*)tmpstr,&usr->VPADBASEGetMotorOnRemainingCount);
		//write the adapter init value starting the chain
		int(*HIDWrite)(unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user) = usr->HIDWrite;
		HIDWrite(p_device->handle, usr->buf, 1, my_write_cb, usr);
		return DEVICE_USED;
	}
	return DEVICE_UNUSED;
}
