#include "main.h"
#include "vpad.h"

extern void DoAnalogConv(void *dst, void *src);

int _main(int ret, void *vpad_data) {
	VPADData *dst = (VPADData*)vpad_data;
	unsigned char *src = (unsigned char*)(*(unsigned int*)(&PadMemLoc));
	if(src && src[0] == 0x21 && (src[1] & 0x10)) //do input if verified
	{
		if(src[2] & 1) dst->btn_hold |= BUTTON_A; //gc a
		if(src[2] & 2) dst->btn_hold |= BUTTON_B; //gc b
		if(src[2] & 4) dst->btn_hold |= BUTTON_X; //gc x
		if(src[2] & 8) dst->btn_hold |= BUTTON_Y; //gc y

		if(src[2] & 0x10) dst->btn_hold |= BUTTON_LEFT; //gc dpad left
		if(src[2] & 0x20) dst->btn_hold |= BUTTON_RIGHT; //gc dpad right
		if(src[2] & 0x40) dst->btn_hold |= BUTTON_DOWN; //gc dpad down
		if(src[2] & 0x80) dst->btn_hold |= BUTTON_UP; //gc dpad up

		if(src[3] & 2) //gc z switch
		{
			if(src[3] & 1) dst->btn_hold |= BUTTON_MINUS; //gc start
			if(src[8] & 0x80) dst->btn_hold |= BUTTON_L; //gc l pressed middle
			if(src[9] & 0x80) dst->btn_hold |= BUTTON_R; //gc r pressed middle
		}
		else
		{
			if(src[3] & 1) dst->btn_hold |= BUTTON_PLUS; //gc start
			if(src[8] & 0x80) dst->btn_hold |= BUTTON_ZL; //gc l pressed middle
			if(src[9] & 0x80) dst->btn_hold |= BUTTON_ZR; //gc r pressed middle
		}
		//Jump to pure ASM for the 4 analog values
		DoAnalogConv(&dst->lstick.x, &src[4]);
	}
	return ret;
}
