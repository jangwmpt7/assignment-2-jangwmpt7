#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

static int mounted = 0;

uint32_t helper_op(uint32_t disk, uint32_t block, uint32_t command){
  uint32_t diskID = disk&0xff;
  uint32_t blockID = (block&0xff) << 4;
  uint32_t commandID = (command&0xff) << 12;
  return diskID | blockID | commandID;
}

int mdadm_mount() {
  uint32_t op = helper_op(0, 0, JBOD_MOUNT);
  // Check if the system is already mounted or not.
  if (mounted == 0) {
    jbod_operation(op, NULL); 
    mounted = 1;
    return 1; 
  }
  else { return -1; }
}

int mdadm_unmount() {
  uint32_t op = helper_op(0, 0, JBOD_UNMOUNT);
  // Check if the system is already unmounted or not.
  if (mounted == 1) { 
    jbod_operation(op, NULL);
    mounted = 0;
    return 1; 
  }
  else { return -1; }
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  if ((start_addr + read_len) > 1048576) { return -1;}    //Ensures that the function does not read beyond the valid address space.
  if (read_len > 1024) { return -2; }   //Ensures that the length to be read does not exceed 1024 bytes.
  if (mounted == 0){ return -3; }   //Ensures that the system is mounted before reading.
  if (read_len == 0 && read_buf == NULL) { return 0; }  //If read_len is 0, return 0.
  if (read_buf == NULL) { return -4; }  //Ensures that read_buff is not null before reading.
  
  uint32_t current_addr;
  uint32_t bytesRead = 0;
  for (current_addr = start_addr; current_addr <= start_addr + read_len; current_addr += bytesRead){  //Read read_len bytes from start_address to the destination.
    uint32_t diskID = current_addr / JBOD_DISK_SIZE;
    uint32_t blockID = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    uint32_t op = helper_op(diskID, blockID, JBOD_SEEK_TO_DISK);
    uint32_t blockOffset = current_addr % JBOD_BLOCK_SIZE;
    uint32_t bytesToCopy = JBOD_BLOCK_SIZE - blockOffset;
    uint32_t bytesRemaining = read_len - bytesRead;
    if (bytesToCopy > bytesRemaining) { bytesToCopy = bytesRemaining; }  //If the bytes to copy exceeds bytes remaining, set the former to the latter.
    if (bytesRemaining + blockOffset > JBOD_BLOCK_SIZE) { bytesToCopy = JBOD_BLOCK_SIZE - blockOffset; }  //If the sum of bytes remaining and offset exceeds JBOD block size, set the bytes to copy to JBOD block size minus block offset.
    jbod_operation(op, NULL);
    op = helper_op(diskID, blockID, JBOD_SEEK_TO_BLOCK);
    jbod_operation(op, NULL);
    op = helper_op(diskID, blockID, JBOD_READ_BLOCK);
    uint8_t buf[JBOD_BLOCK_SIZE];
    jbod_operation(op, buf);
    memcpy((read_buf + bytesRead), (buf + blockOffset), bytesToCopy);
    bytesRead += bytesToCopy;
  }
  return read_len;
}
