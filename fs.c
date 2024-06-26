// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {

  //initialize variables and calcute values
  i8 tempBuf[513];//need temp buf so that only approved data is added to real buf
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd); //printf("cusor %d\n", cursor);
  i32 fbn = cursor / 512; //printf("fbn: %d\n", fbn);
  i32 size = bfsGetSize(inum); //printf("\tsize: %d\n",size);
  i32 roomLeft = size - cursor; //roomLeft till end of file
  i32 numbLeft = (roomLeft < numb) ? roomLeft : numb; //number left to read
  i32 totalRead = numbLeft;
  i32 writeStart = 0;

  //loop reading until requested amount is read
  while (numbLeft > 0){
    i32 numRead = (numbLeft <= 512) ? numbLeft : 512; //read 512 unless you have less than a full block to read
    bfsRead(inum,fbn,tempBuf); //read from file into temp buf
    transplant(buf, tempBuf, writeStart, writeStart%512, writeStart + numRead);
    fsSeek(fd, numRead, SEEK_CUR); //update cursor position
    numbLeft = numbLeft - 512;
    fbn ++;
    writeStart = writeStart + numRead;
  }
                                  
  return totalRead;
}

// ============================================================================
// copy and paste a selected region from one buf to another
// ============================================================================
void transplant(i8* recipient, i8* donor, int recipientStart, int donorStart, 
int recipientEnd) {

  i32 donorIndex = donorStart;
  i32 recipientIndex = recipientStart;
  int counter = 0;
  while (counter < recipientEnd){
    recipient[recipientIndex] = donor[donorIndex];
    donorIndex++;
    recipientIndex++;
    counter++;
  }
}

// ============================================================================
// A method to view the contents of a Buffer
// ============================================================================
void viewBuf(i8* buf){
  //printf("\tbuf: \n");
  //size_t numElements = sizeof(buf) / sizeof(buf[0]);
  for(int i = 0; i < 700; i++){
    //printf("%d,",buf[i]);
    printf("%d\t%d\n",buf[i],i);
  }
}

// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}

// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}

// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}

// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {
  i8 tempBuf[512];
  memset(tempBuf,0,512);
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd); //printf("\tcusor %d\n", cursor);
  i32 writeStart = cursor % 512;// printf("\tstart %d\n", writeStart);
  i32 fbn = cursor / 512; //printf("fbn: %d\n", fbn);
  i32 numbLeft = numb; //keep track of how much more needs to be written
  i32 numWritten = 0;

  //if cursor is not at the front of the block
  if (writeStart != 0){
    numWritten = (numb + writeStart <= 512) ? numb : 512 - writeStart; //printf("\tnumWritten: %d\n", numWritten);
    i32 dbn = bfsFbnToDbn(inum, fbn); //printf("dbn: %d\n", dbn);
    bfsRead(inum, fbn, tempBuf);
    i32 blockFront = fbn * 512; //printf("blockFront: %d\n", blockFront);
    transplant(tempBuf, buf, writeStart, 0, numWritten);
    //update cusor to write from the front of mem block
    fsSeek(fd, blockFront, SEEK_SET);
    bioWrite(dbn, tempBuf);
    numbLeft = numbLeft - numWritten; //printf("\tnumbLeft: %d\n", numbLeft);
    fsSeek(fd, blockFront + numWritten + writeStart, SEEK_SET); //update cursor to reflect what has been written
    fbn++;
  }

  //loop for writing while a whole block is left to write
  while (numbLeft >= 512){
    int count = 1;
    if (numbLeft <= 0){break;} //printf("\tnumbLeft: %d\n",numbLeft);
    i32 size = bfsGetSize(inum); //printf("\tsize: %d\n",size);
    cursor = bfsTell(fd); //printf("\tcusor(2): %d\n", cursor);
    i32 roomLeft = size - cursor; //printf("\troomLeft: %d\n", roomLeft);
    i32 roomNeeded = numbLeft - roomLeft;
    //extend if there isn't enough room to write before end of file
    if (roomLeft < numbLeft){
      bfsExtend(inum, fbn + 1);
      bfsSetSize(inum, size + roomNeeded);
      bfsAllocBlock(inum, fbn + 1);
      size = bfsGetSize(inum); //printf("\tsize: %d\n",size);
    }
    i32 dbn = bfsFbnToDbn(inum, fbn);
    transplant(tempBuf, buf, 0, numWritten, 512); //gather block from larger buffer
    bioWrite(dbn, tempBuf); 
    fbn ++;
    fsSeek(fd, 512, SEEK_CUR);
    numbLeft = numbLeft - 512; //printf("\tnumbLeft: %d\n", numbLeft);
    numWritten += 512;
  }

  //if at the end of the write you need to write less that a full block
  if (numbLeft > 0){
    i32 dbn = bfsFbnToDbn(inum, fbn);
    bfsRead(inum, fbn, tempBuf);
    transplant(tempBuf, buf, 0, numbLeft, numbLeft); //gather block from larger buffer
    bioWrite(dbn, tempBuf);
    fbn ++;
    fsSeek(fd, numbLeft, SEEK_CUR);
    numbLeft = numbLeft - 512;
  }                              
  return 0;
}
