/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 */
#define HAVE_UTYPES
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <device_lock.h>

#include "rtl8710b_ota.h"

#undef LOG_INFO
#undef LOG_DEBUG
#undef CONTAINER_OF
#include <ayla/utypes.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>

#include <ada/libada.h>
#include <ada/client_ota.h>
#include <net/base64.h>
#include <net/net.h>
#include "conf.h"
#include <flash_api.h>


#if defined(CONFIG_PLATFORM_8711B)
int ayla_size = 0; 
char *HeadBuffer = NULL;
static u32 aylanewImg2Addr = 0xFFFFFFFF;
static u32 aylaoldImg2Addr = 0xFFFFFFFF;

u8 ayla_signature[9] = {0};
//extern const update_file_img_id OtaImgId[2];
uint32_t ayla_ota_target_index = OTA_INDEX_2;
update_ota_target_hdr OtaTargetHdr;
extern const update_file_img_id OtaImgId[2];
struct ayla_demo_ota{
	u32 exp_len;
	u32 recv_total_len;
	u32 address;
	s32 remain_bytes;
	u16 hdr_count;
	u8   is_get_ota_hdr;
	u8   ota_exit;
	u8   sig_flg;	/*skip the signature*/
	u8   sig_cnt;
	u8   sig_end;
	u8 save_done;
};
static struct ayla_demo_ota ayla_ota;

static enum patch_state demo_ota_notify(unsigned int len, const char *ver)
{
	log_put(LOG_INFO "OTA notification: length %u version \"%s\"\r\n",
	    len, ver);

	if (len < 4) {
		return PB_ERR_PHEAD;
	}

	ayla_ota.exp_len = len;
	ayla_ota.recv_total_len = 0;
	ayla_ota.save_done = 0;

#ifdef AYLA_DEMO_TEST
	ada_api_call(ADA_OTA_START, OTA_HOST);
#else
	ada_ota_start(OTA_HOST);
#endif
	return PB_DONE;
}

int check_ota_index()
{
	u32 ota_index = -1;
	/* check OTA index we should update */
	if (ota_get_cur_index() == OTA_INDEX_1) {
		ota_index = OTA_INDEX_2;
		log_put(LOG_INFO "OTA2 address space will be upgraded\n");
	} else {
		ota_index = OTA_INDEX_1;
		log_put(LOG_INFO "OTA1 address space will be upgraded\n");
	}
	return ota_index;
}


static enum patch_state demo_ota_save(unsigned int offset, const void *buf,
	size_t len)
{
	flash_t flash_ota;
	u32 exp_len;
	u32 i, k, flash_checksum;
	int ret;
	char *buffer;
	flash_t	flash;
	update_file_hdr OtaFileHdr;
	static u8 ota_tick_log = 0;
	static unsigned long tick1=0, tick2=0;
	if(ayla_ota.ota_exit) {
		log_put(LOG_ERR "Failed to save OTA firmware\n");
		return PB_DONE;
	}
	if (ota_tick_log == 0) {
		tick1 = xTaskGetTickCount();
		ota_tick_log = 1;
	}

	if (ayla_ota.recv_total_len == 0) {
		ayla_ota_target_index = check_ota_index();
		//ayla_ota.partition_addr = update_ota_prepare_addr();
		/* Since debug message in update_ serial function without \n */
		//printf("\n");
		//if (ayla_ota.partition_addr == ~0) {
		//	return PB_ERR_STATE;
		//}
		HeadBuffer = pvPortMalloc(1024);
		if (HeadBuffer == NULL) {
			printf("malloc headbuffer failed\n");
			return PB_ERR_STATE;
		}
	}
	
	buffer = HeadBuffer + ayla_ota.recv_total_len;
	ayla_ota.recv_total_len += len;

	if (ayla_ota.recv_total_len < 128) {
		memcpy(buffer, buf, len);
		ayla_ota.hdr_count += len;
		return PB_DONE;
		
	} else if (ayla_ota.recv_total_len >= 128 && ayla_ota.recv_total_len < 1024) {
		if (ayla_ota.is_get_ota_hdr == 0) {
			buffer = HeadBuffer;
			memcpy(buffer+ayla_ota.hdr_count, buf, ayla_ota.recv_total_len - ayla_ota.hdr_count);
			memcpy((u8*)(&OtaTargetHdr.FileHdr), buffer, sizeof(OtaTargetHdr.FileHdr));
			memcpy((u8*)(&OtaTargetHdr.FileImgHdr), buffer+8, 8);
			/*------acquire the desired Header info from buffer*/
			buffer = HeadBuffer;
			u8 *TempBuf = NULL;
			TempBuf = (u8 *)(&OtaImgId[ayla_ota_target_index]);
			log_put(LOG_INFO "TempBuf = %s\n",TempBuf);
			if (!get_ota_tartget_header(buffer, ayla_ota.recv_total_len, &OtaTargetHdr, TempBuf)) {
				printf("\n\rget OTA header failed\n");
				goto update_ota_exit;
			}

			/*get new image addr and check new address validity*/
			if(!get_ota_address(ayla_ota_target_index, &aylanewImg2Addr, &OtaTargetHdr)) {
				printf("\n get OTA address failed\n");
				goto update_ota_exit;
			}
			/*get new image length from the firmware header*/
			uint32_t NewImg2Len = 0, NewImg2BlkSize = 0;
			NewImg2Len = OtaTargetHdr.FileImgHdr.ImgLen;
			NewImg2BlkSize = ((NewImg2Len - 1)/4096) + 1;
			/*-------------------erase flash space for new firmware--------------*/
			erase_ota_target_flash(aylanewImg2Addr, NewImg2Len);
			
			/*the upgrade space should be masked, because the encrypt firmware is used 
			for checksum calculation*/
			OTF_Mask(1, (aylanewImg2Addr - SPI_FLASH_BASE), NewImg2BlkSize, 1);
			/* get OTA image and Write New Image to flash, skip the signature, 
			not write signature first for power down protection*/
			ayla_ota.address = aylanewImg2Addr -SPI_FLASH_BASE + 8;
			log_put(LOG_INFO "NewImg2Addr = %x\n", aylanewImg2Addr);
			ayla_ota.remain_bytes = OtaTargetHdr.FileImgHdr.ImgLen - 8;
			ayla_ota.is_get_ota_hdr = 1;
			if (HeadBuffer != NULL) {
				vPortFree(HeadBuffer);
				HeadBuffer = NULL;
			}
		}
	}
	/*download the new firmware from server*/
	if(ayla_ota.remain_bytes > 0) {
		if(ayla_ota.recv_total_len > OtaTargetHdr.FileImgHdr.Offset) {
			if(!ayla_ota.sig_flg) {
				u32 Cnt = 0;
				/*reach the the desired image, the first packet process*/
				ayla_ota.sig_flg = 1;
				Cnt = ayla_ota.recv_total_len -OtaTargetHdr.FileImgHdr.Offset;
				if(Cnt < 8) {
					ayla_ota.sig_cnt = Cnt;
				} else {
					ayla_ota.sig_cnt = 8;
				}
				memcpy(ayla_signature, (char*)buf + len - Cnt, ayla_ota.sig_cnt);
				if((ayla_ota.sig_cnt < 8) || (Cnt -8 == 0)) {
					return PB_DONE;
				}				
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash, ayla_ota.address + ayla_size, Cnt -8, (char*)buf + (len -Cnt + 8)  ) < 0){
					log_put(LOG_ERR "Write sector failed");
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				ayla_size += (Cnt - 8);
				ayla_ota.remain_bytes -= ayla_size;
			} else {					
				/*normal packet process*/
				if(ayla_ota.sig_cnt < 8) {
					if(len < (8 -ayla_ota.sig_cnt)) {
						memcpy(ayla_signature + ayla_ota.sig_cnt, buf, len);
						ayla_ota.sig_cnt += len;
						return PB_DONE;
					} else {
						memcpy(ayla_signature + ayla_ota.sig_cnt, buf, (8 - ayla_ota.sig_cnt));
						ayla_ota.sig_end = 8 - ayla_ota.sig_cnt;
						len -= (8 - ayla_ota.sig_cnt) ;
						ayla_ota.sig_cnt = 8;
						if(!len) {
							return PB_DONE;
						}
						ayla_ota.remain_bytes -= len;
						if (ayla_ota.remain_bytes <= 0) {
							len = len - (-ayla_ota.remain_bytes);
						}
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						if (flash_stream_write(&flash, ayla_ota.address+ayla_size, len, (char*)buf + ayla_ota.sig_end) < 0){
							log_put(LOG_ERR "Write sector failed");
							device_mutex_unlock(RT_DEV_LOCK_FLASH);
							goto update_ota_exit;
						}
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						ayla_size += len;
						return PB_DONE;
					}
				}
				ayla_ota.remain_bytes -= len;
				int end_ota = 0;
				if(ayla_ota.remain_bytes <= 0) {
					len = len - (-ayla_ota.remain_bytes);
					end_ota = 1;
				}
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash, ayla_ota.address + ayla_size, len, buf) < 0){
					log_put(LOG_ERR "Write sector failed");
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				ayla_size += len;
				if (end_ota) {
					log_put(LOG_INFO "size = %x\n", ayla_size);
					ayla_ota.save_done = 1;
					/*------------- verify new firmware checksum-----------------*/
					log_put(LOG_INFO, "size = %d, OtaTargetHdr.FileImgHdr.ImgLen = %d\n", ayla_size, OtaTargetHdr.FileImgHdr.ImgLen);
					log_put(LOG_INFO, "buffer signature is: = %s", ayla_signature);
					 /*------------- verify checksum and update signature-----------------*/
					if(verify_ota_checksum(aylanewImg2Addr, ayla_size, ayla_signature, &OtaTargetHdr)){
						if(!change_ota_signature(aylanewImg2Addr, ayla_signature, ayla_ota_target_index)) {
							log_put(LOG_ERR "\n%s: change signature failed\n");
							return PB_ERR_FATAL;
						}
						return PB_DONE;
					} else {
						/*if checksum error, clear the signature zone which has been 
						written in flash in case of boot from the wrong firmware*/
						#if 1
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						flash_erase_sector(&flash, aylanewImg2Addr - SPI_FLASH_BASE);
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						#endif
						log_put(LOG_ERR "chack_sum error\n");
						return PB_ERR_FATAL;
					}

				}
				tick2 = xTaskGetTickCount();
				if (tick2 - tick1 > 500) {
					log_put(LOG_INFO "Download OTA file: %d B, RemainBytes = %d\n", (ayla_size), ayla_ota.remain_bytes);
					ota_tick_log = 0;
				}
			}
		}
	}
	return PB_DONE;
	
update_ota_exit:
	if (HeadBuffer != NULL) {
		vPortFree(HeadBuffer);
		HeadBuffer = NULL;
	}
	ayla_ota.ota_exit = 1;
	memset(&ayla_ota, 0, sizeof(ayla_ota));	
	log_put(LOG_ERR "Update task exit");	
	return PB_ERR_FATAL;	
}

static void demo_ota_save_done(void)
{
	enum patch_state status;

	if (!ayla_ota.save_done) {
		log_put(LOG_WARN "OTA save_done: OTA not completely saved");
		status = PB_ERR_FATAL;
#ifdef AYLA_DEMO_TEST
		ada_api_call(ADA_OTA_REPORT, OTA_HOST, status);
#else
		ada_ota_report(OTA_HOST, status);
#endif
		return;
	}

	vTaskDelay(200);
	ota_platform_reset();
}

static struct ada_ota_ops demo_ota_ops = {
	.notify = demo_ota_notify,
	.save = demo_ota_save,
	.save_done = demo_ota_save_done,
};

void demo_ota_init(void)
{
	ada_ota_register(OTA_HOST, &demo_ota_ops);
}

#endif

