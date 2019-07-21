#include "lib.h"

#define MAX_OBJS 1000000
#define IMAP_OFF 2
#define NUM_IMAP_BLOCKS 32
#define NUM_DMAP_BLOCKS 256
#define NUM_HASH_BLOCKS 15000
#define DIRECT_LT 12*BLOCK_SIZE
#define MAX_FILE_SIZE BLOCK_SIZE*BLOCK_SIZE

static struct objfs_state *objfs;

// Credits for hash function:   www.cse.yorku.ca/~oz/hash.html
typedef unsigned long u64;

unsigned long hash(const char *str)
{
    u64 hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    hash = hash % MAX_OBJS;
    return hash;
}

// Inode Structure:
typedef struct object{
     long id;
     long size;
     int cache_index;
     unsigned long direct[12];
     unsigned long indirect[8];
     int dirty;
     char key[32];
}object;

// 2 power 8 for data and 8 blocks
// hashtable node
typedef struct hash_node{
  // 0 -> free to use
  // 1 -> something valid is there
  // 2 -> deleted
  char curr_status;
  unsigned long id;
  char key[32];
}hnode;

typedef struct data{
  hnode **hashmap;
  u64 **i_bitmap;
  u64 **d_bitmap;
}data;
    
// hashtable

struct object *objs;

#define my_malloc(x, size) do{\
                         (x) = mmap(NULL, (size), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0);

#define malloc_4k(x) do{\
                         (x) = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_4k(x) munmap((x), BLOCK_SIZE)

#ifdef CACHE         // CACHED implementation
static void init_object_cached(struct object *obj)
{
           obj->cache_index = -1;
           obj->dirty = 0;
           return;
}
static void remove_object_cached(struct object *obj)
{
           obj->cache_index = -1;
           obj->dirty = 0;
          return;
}
static int find_read_cached(struct objfs_state *objfs, struct object *obj, char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(user_buf, cache_ptr, size);
         return 0;
}
/*Find the object in the cache and update it*/
static int find_write_cached(struct objfs_state *objfs, struct object *obj, const char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(cache_ptr, user_buf, size);
         obj->dirty = 1;
         return 0;
}
/*Sync the cached block to the disk if it is dirty*/
static int obj_sync(struct objfs_state *objfs, struct object *obj)
{
   char *cache_ptr = objfs->cache + (obj->id << 12);
   if(!obj->dirty)
      return 0;
   if(write_block(objfs, obj->id, cache_ptr) < 0)
      return -1;
    obj->dirty = 0;
    return 0;
}
#else  //uncached implementation
static void init_object_cached(struct object *obj)
{
   return;
}
static void remove_object_cached(struct object *obj)
{
     return ;
}
static int find_read_cached(struct objfs_state *objfs, struct object *obj, char *user_buf, int size)
{
   void *ptr;
   malloc_4k(ptr);
   if(!ptr)
        return -1;
   if(read_block(objfs, obj->id, ptr) < 0)
       return -1;
   memcpy(user_buf, ptr, size);
   free_4k(ptr);
   return 0;
}
static int find_write_cached(struct objfs_state *objfs, struct object *obj, const char *user_buf, int size)
{
   void *ptr;
   malloc_4k(ptr);
   if(!ptr)
        return -1;
   memcpy(ptr, user_buf, size);
   if(write_block(objfs, obj->id, ptr) < 0)
       return -1;
   free_4k(ptr);
   return 0;
}
static int obj_sync(struct objfs_state *objfs, struct object *obj)
{
   return 0;
}
#endif

// Returns minimum of a and b
u64 min(u64 a, u64 b){
  u64 mini = (a<b) ? a:b;
  return mini;
}

// Returns minimum of a and b
u64 max(u64 a, u64 b){
  u64 maxi = (a>b) ? a:b;
  return maxi;
}

long set_data_bitmap_0( struct objfs_state *objfs, long block_num){
  u64 **d_bitmap = ((data*)(objfs->objstore_data))->d_bitmap;

  u64 i = block_num / (BLOCK_SIZE*8);
  u64 j = (block_num % (BLOCK_SIZE*8)) / 64;
  u64 k = block_num % 64; 

  d_bitmap[i][j] &= ~(1<<(63-k));
  return 0;
}

long set_inode_bitmap_0( struct objfs_state *objfs, long block_num){
  u64 **i_bitmap = ((data*)(objfs->objstore_data))->i_bitmap;

  u64 i = block_num / (BLOCK_SIZE*8);
  u64 j = (block_num % (BLOCK_SIZE*8)) / 64;
  u64 k = block_num % 64; 

  i_bitmap[i][j] &= ~(1<<(63-k));
  return 0;
}

// Checks if valid data block to read from
long check_data_bitmap( struct objfs_state *objfs, long block_num){
  u64 **d_bitmap = ((data*)(objfs->objstore_data))->d_bitmap;

  u64 i = block_num / (BLOCK_SIZE*8);
  u64 j = (block_num % (BLOCK_SIZE*8)) / 64;
  u64 k = block_num % 64; 

  if((d_bitmap[i][j] & (1<<(63-k))) != 0){
    return 1;
  }
  else
    return 0;
}

// Sets data bitmap and returns the one set
long set_data_bitmap( struct objfs_state *objfs){

  u64 **d_bitmap = ((data*)(objfs->objstore_data))->d_bitmap;  
  u64 num_entry_in_row = BLOCK_SIZE / sizeof(u64);

  for(int i=0;i<NUM_DMAP_BLOCKS;i++){
    for(int j=0;j<num_entry_in_row;j++){

      if(d_bitmap[i][j] != -1) {
        u64 tmp = d_bitmap[i][j];
        for(int k=0;k<64;k++){

          if(tmp%2 == 0){
            d_bitmap[i][j] |= 1<<k;
            u64 data_blk = (i*num_entry_in_row + j)*64 + 63 - k;
            return data_blk;
          }
          else
            tmp /= 2;
          }

      }
    }
  }
  return -1;
}

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
  u64 key_idx = hash(key);
  hnode **hashmap = ((data*)(objfs->objstore_data))->hashmap;

  u64 num_hnode_in_row = BLOCK_SIZE / sizeof(hnode);

  u64 block_num = key_idx / num_hnode_in_row;
  u64 block_offset = key_idx % num_hnode_in_row;

  for(int i=block_num;i==block_num;i++){
    for(int j=block_offset;j<num_hnode_in_row;j++){ 
      if(hashmap[i][j].curr_status == 0)
        return -1;
      if((hashmap[i][j].curr_status == 1) && (strcmp(hashmap[i][j].key, key) == 0 )){
        return hashmap[i][j].id;
      }
    }
  }

  for(int i=block_num+1;i<NUM_HASH_BLOCKS;i++){
    for(int j=0;j<num_hnode_in_row;j++){
      if(hashmap[i][j].curr_status == 0)
        return -1;
      if((hashmap[i][j].curr_status == 1) && (strcmp(hashmap[i][j].key, key) == 0 )){
        return hashmap[i][j].id;
      }
    }
  }

  for(int i=0;i<block_num;i++){
    for(int j=0;j<num_hnode_in_row;j++){
      if(hashmap[i][j].curr_status == 0)
        return -1;
      if((hashmap[i][j].curr_status == 1) && (strcmp(hashmap[i][j].key, key) == 0 )){
        return hashmap[i][j].id;
      }
    }
  }

  for(int i=block_num;i==block_num;i++){
    for(int j=0;j<block_offset;j++){
      if(hashmap[i][j].curr_status == 0)
        return -1;
      if((hashmap[i][j].curr_status == 1) && (strcmp(hashmap[i][j].key, key) == 0 )){
        return hashmap[i][j].id;
      }
    }
  }
  return -1;
}



long add_entry( struct objfs_state *objfs, const char *key, u64 id){
  hnode **hashmap = ((data*)(objfs->objstore_data))->hashmap;
  // Get hashed key
  u64 hashed = hash(key);

  u64 num_hnode_in_row = BLOCK_SIZE / sizeof(hnode);
  for(int k=hashed;k<NUM_HASH_BLOCKS*num_hnode_in_row;k++){
    u64 i = k / num_hnode_in_row;
    u64 j = k % num_hnode_in_row;
    if(hashmap[i][j].curr_status != 1){
      hashmap[i][j].curr_status = 1;
      strcpy(hashmap[i][j].key,key);
      hashmap[i][j].id = id;
      return k; 
    }
  }

  for(int k=0;k<hashed;k++){
    u64 i = k / num_hnode_in_row;
    u64 j = k % num_hnode_in_row;
    if(hashmap[i][j].curr_status != 1){
      hashmap[i][j].curr_status = 1;
      strcpy(hashmap[i][j].key,key);
      hashmap[i][j].id = id;
      return k; 
    }
  }

  return -1;
}

long delete_entry( struct objfs_state *objfs, const char *key, u64 id){

  hnode **hashmap = ((data*)(objfs->objstore_data))->hashmap;
  // Get hashed key
  u64 hashed = hash(key);
  u64 num_hnode_in_row = BLOCK_SIZE / sizeof(hnode);
  u64 j = hashed % num_hnode_in_row;  

  for(int k=hashed; k<NUM_HASH_BLOCKS*num_hnode_in_row; k++){
    u64 i = k / num_hnode_in_row;
    u64 j = k % num_hnode_in_row;
    if(hashmap[i][j].curr_status == 1 && (strcmp(hashmap[i][j].key, key) == 0)){
      hashmap[i][j].curr_status = 2;
      return 0;
    }
    else if(hashmap[i][j].curr_status == 0){
      return -1;
    }
  }

  for(int k=0; k<hashed; k++){
    u64 i = k / num_hnode_in_row;
    u64 j = k % num_hnode_in_row;
    if(hashmap[i][j].curr_status == 1 && (strcmp(hashmap[i][j].key, key) == 0)){
      hashmap[i][j].curr_status = 2;
      return 0; 
    }
    else if(hashmap[i][j].curr_status == 0)
      return -1;
  }
  return -1;
}

u64 write_inode_meta(struct objfs_state *objfs, const char *key, u64 objno){
  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objno / iblocks_in_row;
  void *buf;
  malloc_4k(buf);
  if(read_block(objfs, curr_offset, (char *)buf) < 0)
    return -1;
  object *inode_data = (object *)buf;
  inode_data += objno % iblocks_in_row;

  // Set inode metadata
  inode_data->size = 0;
  strcpy(inode_data->key, key);
  inode_data->id = objno;
  inode_data->dirty = 0;
  for(int i=0;i<12;i++)
    inode_data->direct[i] = -1;
  for(int i=0;i<8;i++)
    inode_data->indirect[i] = -1;

  if(write_block(objfs, curr_offset, (char *)buf) < 0)
    return -1;
  free_4k(buf);
  return objno;
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long create_object(const char *key, struct objfs_state *objfs)
{
    if( find_object_id(key,objfs) >= 0){
      return -1;
    }

    u64 **i_bitmap = ((data*)(objfs->objstore_data))->i_bitmap;
    hnode **hashmap = ((data*)(objfs->objstore_data))->hashmap;   
    u64 num_entry_in_row = BLOCK_SIZE / sizeof(u64);

    for(int i=0;i<NUM_IMAP_BLOCKS;i++){
      for(int j=0;j<num_entry_in_row;j++){

        if(i_bitmap[i][j] != -1 ){
          u64 tmp = i_bitmap[i][j];
          for(int k=0;k<64;k++){

            if(tmp%2 == 0){
              i_bitmap[i][j] |= 1<<k;
              u64 objno = (i*num_entry_in_row + j)*64 + 63 - k;

              // Add entry into hash table
              int b;
              if((b=add_entry(objfs, key, objno)) < 0){
                return -1; 
              }
              else{
                return write_inode_meta(objfs, key, objno);
              }
            }
            else
              tmp /= 2;
          }
        }
      }
    }
    return -1;
}
/*
  One of the users of the object has dropped a reference
  Can be useful to implement caching.
  Return value: Success --> 0
                Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs)
{
    // dprintf("Release reached\n");
    return 0;
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
long destroy_object(const char *key, struct objfs_state *objfs)
{
  // dprintf("-------------------------------------\n");
  long objid;
  if((objid = find_object_id(key, objfs)) < 2)
      return -1;

  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objid / iblocks_in_row;
  void *buffer;
  malloc_4k(buffer);
  if(read_block(objfs, curr_offset, (char *)buffer) < 0)
    return -1;
  object *inode_data = (object*)buffer;
  inode_data += objid % iblocks_in_row;

  int orig_size = inode_data -> size;
  int size_remaining = orig_size;
  int offset = 0;

  while(size_remaining > 0){
    if(offset < DIRECT_LT){
      u64 i = offset / BLOCK_SIZE;  // within direct offset

      u64 direct_offset = inode_data->direct[i];
      inode_data->direct[i] = 0;
      // dprintf("entered destroy: direct\n");
      set_data_bitmap_0(objfs,direct_offset);
    }
    // if outside direct pointers region
    else{
      u64 i = (offset - DIRECT_LT);
      u64 idx = i/(BLOCK_SIZE * (BLOCK_SIZE / 8));  // this gives indirect index to read from
      u64 within_blk_offset_1 = i % (BLOCK_SIZE * (BLOCK_SIZE / 8));
      u64 indirect_offset = inode_data->indirect[idx];

      // Read the indirect block
      set_data_bitmap_0(objfs,indirect_offset);
      void *buffer1;
      malloc_4k(buffer1);
      if(read_block(objfs, indirect_offset, (char *)buffer1) < 0)
        return -1;

      u64 final_blk_idx = ((u64*) buffer1)[within_blk_offset_1];

      set_data_bitmap_0(objfs,final_blk_idx);

      free_4k(buffer1);
    }
    offset += BLOCK_SIZE;
    size_remaining -= BLOCK_SIZE;
  }

    // Set inode metadata
  inode_data->size = 0;
  strcpy(inode_data->key, "");
  inode_data->id = -1;
  inode_data->dirty = 0;
  for(int i=0;i<12;i++)
    inode_data->direct[i] = 0;
  for(int i=0;i<8;i++)
    inode_data->indirect[i] = 0;

  if(write_block(objfs, curr_offset, (char *)buffer) < 0)
    return -1;

  set_inode_bitmap_0(objfs,objid);

  delete_entry(objfs,key,objid);
  return 0;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.  
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
  long objno;
    // New key already exist
  if((objno = find_object_id(newname, objfs)) >= 0)
        return -1;
 
  if((objno = find_object_id(key, objfs)) < 0)
        return -1;
  if(strlen(newname) > 32)
    return -1;

  // Delete the previous entry in hashtable
  if(delete_entry(objfs,key,objno) < 0)
    return -1;
  // Delete the previous entry in hashtable
  if(add_entry(objfs,newname,objno) < 0)
    return -1;

  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objno / iblocks_in_row;
  void *buf;
  malloc_4k(buf);
  if(read_block(objfs, curr_offset, (char *)buf) < 0)
    return -1;
  object *inode_data = buf;
  inode_data += objno % iblocks_in_row;

  // Set inode metadate
  strcpy(inode_data->key , newname);
  inode_data -> dirty = 1;

  if(write_block(objfs, curr_offset, (char *)buf) < 0)
    return -1;
  free_4k(buf);

  return objno;
}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs, off_t offset)
{
  // Conditions to return
  if(offset < 0)
    return -1;
  if(objid < 2)
    return -1;
  if(offset + size > MAX_FILE_SIZE)
    return -1;

  int objno = objid;

  // Access that inode
  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objno / iblocks_in_row;
  void *buffer;
  malloc_4k(buffer);
  if(read_block(objfs, curr_offset, (char *)buffer) < 0)
    return -1;
  object *inode_data = buffer;
  inode_data += objno % iblocks_in_row;
  // Agar file ka size kam ho jahan pe write karna hai usse
  if(inode_data->size < offset)
    return -1;

  u64 orig_size = inode_data->size;
  int set;
  int size_remaining = size;
  int size_written = 0;

  // Check if within DATA_lIM
  while(size_remaining > 0){
    int write_size = 0;
    if(offset < DIRECT_LT){
      u64 i = offset / BLOCK_SIZE;  // within direct offset

      if(inode_data->direct[i] == -1){
        set = set_data_bitmap(objfs);
        u64 a = check_data_bitmap(objfs,set);
        if(set < 0){
          return -1;
        }
        inode_data->direct[i] = set;
        if(write_block(objfs, curr_offset , (char *)buffer) < 0)
          return -1;
      }

      u64 within_blk_offset = offset % BLOCK_SIZE;

      void *buffer1;
      malloc_4k(buffer1);
      // First read from data block
      if(read_block(objfs, inode_data->direct[i] , (char *)buffer1) < 0)
        return -1;

      write_size = min(size_remaining, BLOCK_SIZE - within_blk_offset);
      memcpy( buffer1 + within_blk_offset, buf+size_written, write_size);

      if(write_block(objfs, inode_data->direct[i] , (char *)buffer1) < 0)
        return -1;

      free_4k(buffer1);
    }
    // if outside direct pointers region
    else{
      u64 i = (offset - DIRECT_LT);
      u64 idx = (i*BLOCK_SIZE) * (BLOCK_SIZE / 8);  // this gives indirect index to read from
      u64 within_blk_offset_1 = (i/BLOCK_SIZE) % (BLOCK_SIZE / 8);
      u64 indirect_offset = inode_data->indirect[idx];

      // Check if this data block is already allocated using bitmap and set bitmap
      if(indirect_offset == -1){
        u64 set = set_data_bitmap(objfs);
        if(set < 0){
          return -1;
        }
        inode_data->indirect[idx] = set;
        indirect_offset = set;
        if(write_block(objfs, curr_offset , (char *)buffer) < 0)
          return -1;
      }
      // Read the indirect block
      void *buffer1;
      malloc_4k(buffer1);
      if(read_block(objfs, indirect_offset, (char *)buffer1) < 0)
        return -1;

      int final_blk_idx = ((u64*) buffer1)[within_blk_offset_1];

      if(final_blk_idx<=0 || offset >= orig_size){
        u64 set = set_data_bitmap(objfs);
        if(set < 0){
          return -1;
        }
        final_blk_idx = set;
        ((u64*) buffer1)[within_blk_offset_1] = set;
        if(write_block(objfs, indirect_offset , (char *)buffer1) < 0)
          return -1;
      }

      void *buffer2;
      malloc_4k(buffer2);
      if(read_block(objfs, final_blk_idx, (char *)buffer2) < 0)
        return -1;

      // Read the block from which to read from
      u64 within_blk_offset_2 = offset % BLOCK_SIZE;
      write_size = min(size_remaining, BLOCK_SIZE - within_blk_offset_2);

      memcpy( buffer2 + within_blk_offset_2, buf+size_written, write_size);

      if(write_block(objfs, inode_data->direct[i] , (char *)buffer2) < 0)
        return -1;

      free_4k(buffer2);
      free_4k(buffer1);
    }
    size_written += write_size;
    offset += write_size;
    size_remaining -= write_size;
    inode_data -> size = max(inode_data->size, offset);
    if(write_block(objfs, curr_offset , (char *)buffer) < 0)
      return -1;
  }
  return size_written;
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs, off_t offset)
{
  if(offset < 0)
    return -1;
  if(objid < 2)
    return -1;
  int objno = objid;

  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objno / iblocks_in_row;
  void *buffer;
  malloc_4k(buffer);
  if(read_block(objfs, curr_offset, (char *)buffer) < 0)
    return -1;
  object *inode_data = buffer;
  inode_data += objno % iblocks_in_row;

  // Agar file ka size kam ho jahan se read karna hai usse
  if(inode_data->size < offset)
    return -1;

  int size_remaining = size;
  int size_read = 0;

  // Check if within DATA_lIM
  while(size_remaining > 0){
    if(offset >= inode_data->size)
      return size_read;
    int read_size = 0;
    if(offset < DIRECT_LT){
      u64 i = offset / BLOCK_SIZE;  // within direct offset
      u64 direct_offset = inode_data->direct[i];
      u64 within_blk_offset = offset % BLOCK_SIZE;
      if(check_data_bitmap(objfs, direct_offset) == 0) {
        return size_read;
      } 

      object *buffer1;
      malloc_4k(buffer1);
      if(read_block(objfs, direct_offset, (char *)buffer1) < 0)
        return -1;
      read_size = min(size_remaining, BLOCK_SIZE - within_blk_offset);
      read_size = min(read_size, inode_data->size - offset);
      memcpy(buf+size_read, buffer1 + within_blk_offset, read_size);
      free_4k(buffer1);
    }
    // if outside direct pointers region
    else{
      u64 i = (offset - DIRECT_LT);
      u64 idx = (i*BLOCK_SIZE) / ((BLOCK_SIZE / 8));  // this gives indirect index to read from
      u64 within_blk_offset_1 = (i/BLOCK_SIZE) % (BLOCK_SIZE / 8);
      u64 indirect_offset = inode_data->indirect[idx];
      // Check if this data block is already allocated using bitmap
      if(check_data_bitmap(objfs, indirect_offset) == 0)  {
        return size_read;
      }
      // Read the indirect block
      void *buffer1;
      malloc_4k(buffer1);
      if(read_block(objfs, indirect_offset, (char *)buffer1) < 0)
        return -1;

      u64 final_blk_idx = ((u64*) buffer1)[within_blk_offset_1];
      // Check if this data block is already allocated using bitmap
      if(check_data_bitmap(objfs, final_blk_idx) == 0)  {
        return size_read;
      }

      void *buffer2;
      malloc_4k(buffer2);
      if(read_block(objfs, final_blk_idx, (char *)buffer2) < 0)
        return -1;

      // Read the block from which to read from
      u64 within_blk_offset_2 = offset % BLOCK_SIZE;
      read_size = min(size_remaining, BLOCK_SIZE - within_blk_offset_2);
      read_size = min(read_size, inode_data->size - offset); 
      memcpy(buf+size_read, buffer2 + within_blk_offset_2, read_size);
      free_4k(buffer2);
      free_4k(buffer1);
    }
    size_read += read_size;
    offset += read_size;
    size_remaining -= read_size;
  }
  free_4k(buffer);
   return size_read;
}

/*
  Reads the object metadata for obj->id = objid.
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf, struct objfs_state *objfs)
{
  int objno = buf->st_ino;
  if(objno < 2){
    return -1;
  }

  // Access that inode
  u64 prev_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + NUM_HASH_BLOCKS;
  u64 iblocks_in_row = BLOCK_SIZE / sizeof(object);

  // Get block no. to read and write
  u64 curr_offset = prev_offset + objno / iblocks_in_row;
  void *buffer;
  malloc_4k(buffer);

  if(read_block(objfs, curr_offset, (char *)buffer) < 0){
    return -1;
  }

  object *inode_data = (object *)buffer;
  inode_data += objno % iblocks_in_row;

  buf->st_blocks = (inode_data->size + 511)/512;
  buf->st_size = inode_data->size;
  free_4k(buffer);
  return 0;
}

/*
   Set your private pointeri, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
  u64 **imap;
   // Initilize inode bitmaps
  // 32 blocks for inode bitmaps required
  my_malloc(imap, NUM_IMAP_BLOCKS*sizeof(u64*));
  if(!imap){
           return -1;
       }
  for(int i=0;i<NUM_IMAP_BLOCKS;i++){
    u64* tmp;
    malloc_4k(tmp);
    if(!tmp){
       return -1;
    }
    imap[i] = tmp;
    u64 blk_offset = IMAP_OFF + i;
    if(read_block(objfs, blk_offset, (char *)imap[i]) < 0)
       return -1;
  }


   // Initilize dnode bitmaps
  // 256 blocks for dnode bitmaps required
  u64 **dmap;
  my_malloc(dmap, NUM_DMAP_BLOCKS*sizeof(u64*));
  if(!dmap){
           return -1;
       }
  for(int i=0;i<NUM_DMAP_BLOCKS;i++){
    u64* tmp;
    malloc_4k(tmp);
    if(!tmp){
       return -1;
    }
    dmap[i] = tmp;
    u64 blk_offset = IMAP_OFF + NUM_IMAP_BLOCKS + i;
    if(read_block(objfs, blk_offset, (char *)dmap[i]) < 0)
       return -1;
  }


  // Initilize Hash table
  // 12,000 blocks for dnode bitmaps required
  hnode **hashmap;
  my_malloc(hashmap, NUM_HASH_BLOCKS*sizeof(hnode*));
  if(!hashmap){
       return -1;
   }
  for(int i=0;i<NUM_HASH_BLOCKS;i++){
    hnode* tmp;
    malloc_4k(tmp);
    if(!tmp){
       return -1;
    }
    hashmap[i] = tmp;
    u64 blk_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + i;
    if(read_block(objfs, blk_offset, (char *)hashmap[i]) < 0)
       return -1;
  }
   data *my_data;
   my_malloc(my_data,sizeof(data));
   my_data->i_bitmap = imap;
   my_data->d_bitmap = dmap;
   my_data->hashmap = hashmap;

   objfs->objstore_data = my_data;
   return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
  data* my_data = (objfs->objstore_data);
  u64 **imap = my_data->i_bitmap;
  u64 **dmap = my_data->d_bitmap;
  hnode **hashmap = my_data->hashmap;
   // Initilize inode bitmaps
  // 32 blocks for inode bitmaps required
  for(int i=0;i<NUM_IMAP_BLOCKS;i++){
    u64 blk_offset = IMAP_OFF + i;
    if(write_block(objfs, blk_offset, (char *)imap[i]) < 0)
       return -1;
     free_4k(imap[i]);
  }

   // Initilize dnode bitmaps
  // 256 blocks for dnode bitmaps required
  for(int i=0;i<NUM_DMAP_BLOCKS;i++){
    u64 blk_offset = IMAP_OFF + NUM_IMAP_BLOCKS + i;
    if(write_block(objfs, blk_offset, (char *)dmap[i]) < 0)
       return -1;
     free_4k(dmap[i]);
  }


  // Initilize Hash table
  // 12,000 blocks for dnode bitmaps required
  for(int i=0;i<NUM_HASH_BLOCKS;i++){
    u64 blk_offset = IMAP_OFF + NUM_IMAP_BLOCKS + NUM_DMAP_BLOCKS + i;
    if(write_block(objfs, blk_offset, (char *)hashmap[i]) < 0)
       return -1;
    free_4k(hashmap[i]);
  }

   objfs->objstore_data = NULL;
   return 0;
   return 0;
}