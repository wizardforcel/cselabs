#include "inode_manager.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
using namespace std;


// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  const int start = ((sb.nblocks) / BPB + (sb.ninodes) / IPB + 4);
  for(int i = start; i < sb.nblocks; i++)
  {
    if(using_blocks.find(i) == using_blocks.end())
    {
      using_blocks[i] = 0;
      return i;
    }
  }

  return -1;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  
  std::map<uint32_t, int>::iterator it
    = using_blocks.find(id);
  if(it != using_blocks.end())
    using_blocks.erase(it);

  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

blockid_t inode_manager::getblockid(inode *ino, int index)
{
  if(index >= 0 && index < NDIRECT)
    return ino->blocks[index];
  else if(index < MAXFILE)
  {
    char buff[BLOCK_SIZE];
    bm->read_block(ino->indirblock, buff);
    blockid_t *p = (blockid_t *)buff;
    return p[index - NDIRECT];
  }
  else return -1;
}

void inode_manager::setblockid(inode *ino, int index, blockid_t val)
{
  if(index >= 0 && index < NDIRECT)
    ino->blocks[index] = val;
  else if(index < MAXFILE)
  {
    char buff[BLOCK_SIZE];
    bm->read_block(ino->indirblock, buff);
    blockid_t *p = (blockid_t *)buff;
    p[index - NDIRECT] = val;
    bm->write_block(ino->indirblock, buff);
  }
}

inode_manager::inode_manager()
{
  bm = new block_manager();
  
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if(root_dir != 1) 
  {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

  if(type == extent_protocol::T_FREE) return -1;

  struct inode *ino;

  for(int i = 1; i <= INODE_NUM; i++)
  {
    ino = get_inode(i);
    if(ino->type == extent_protocol::T_FREE)
    {
      int tm = time(0);
      ino->atime = tm;
      ino->ctime = tm;
      ino->mtime = tm;
      ino->type = type;
      ino->size = 0;
      ino->bnum = 0;
      ino->indirblock = bm->alloc_block();
      put_inode(i, ino);
      delete ino;
      return i;
    }
    delete ino;
  }    
  return -1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *ino = get_inode(inum);
  if(ino == 0) return;
  if(ino->type == extent_protocol::T_FREE)
  {
    delete ino;
    return;
  }

  remove_file(inum);
  bzero(ino, sizeof(inode));
  put_inode(inum, ino);  
  delete ino;
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if(inum < 0 || inum >= INODE_NUM) 
  {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);

  if(inum < 0 || inum >= INODE_NUM)
  {
    printf("\tim: inum out of range\n");
    return;
  }
  if(ino == NULL) return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */

  struct inode *ino = get_inode(inum);
  if(ino == 0) return;
  if(ino->type == 0)
  {
    delete ino;
    return;
  }
  ino->atime = time(0);

  cout << "Read Size: " << ino->size << endl;
  char *buf = new char[ino->size];
  for(int i = 0; i < ino->bnum; i++)
  {
    int blockid = getblockid(ino, i);
    char cache[BLOCK_SIZE];
    bm->read_block(blockid, cache);
    for(int j = 0; j < BLOCK_SIZE; j++)
    {
      int ibuf = i * BLOCK_SIZE + j;
      if(ibuf >= ino->size) break;
      buf[ibuf] = cache[j];
    }
  }
  
  *buf_out = buf;
  *size = ino->size;
  delete ino;
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  struct inode *ino = get_inode(inum);
  if(ino == 0) return;
  if(ino->type == 0)
  {
    delete ino;
    return;
  }
  int tm = time(0);
  ino->atime = tm;
  ino->mtime = tm;
  ino->ctime = tm;

  int bneed = size / BLOCK_SIZE + ((size % BLOCK_SIZE)? 1: 0);
  cout << "Write Size: " << size << " BNeed: " << bneed << endl;
  if(ino->bnum > bneed)
  {
    for(int i = ino->bnum - 1; i >= bneed; i--)
    {
      int blockid = getblockid(ino, i);
      bm->free_block(blockid);
    }
    ino->bnum = bneed;
  }
  else if(ino->bnum < bneed)
  {
    for(int i = ino->bnum; i < bneed; i++)
    {
      blockid_t tmp = bm->alloc_block();
      setblockid(ino, i, tmp);
    }
    ino->bnum = bneed;
  }

  for(int i = 0; i < ino->bnum; i++)
  {
    int blockid = getblockid(ino, i);
    char cache[BLOCK_SIZE];
    for(int j = 0; j < BLOCK_SIZE; j++)
    {
      int ibuf = i * BLOCK_SIZE + j;
      if(ibuf >= size) break;
      cache[j] = buf[ibuf];
    }
    bm->write_block(blockid, cache);
  }

  ino->size = size;
  put_inode(inum, ino);
  delete ino;
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  
  struct inode *ino = get_inode(inum);
  if(ino == 0) return;
  if(ino->type == 0)
  {
    delete ino;
    return;
  }
  ino->atime = time(0);

  a.type = ino->type;
  a.size = ino->size;
  a.atime = ino->atime;
  a.ctime = ino->ctime;
  a.mtime = ino->mtime;

  delete ino;
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */

  struct inode *ino = get_inode(inum);
  for(int i = 0; i < ino->bnum; i++)
  {
    bm->free_block(getblockid(ino, i));
  }
  bm->free_block(ino->indirblock);
  delete ino;
}
