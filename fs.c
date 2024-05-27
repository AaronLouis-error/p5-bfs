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
  i8 tempBuf[513];//need temp buf so that only approved data is added to real buf
  //printf("Numb: %d\n", numb);
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd); //printf("cusor %d\n", cursor);
  i32 dataStart = cursor % 512;  //printf("start %d\n", dataStart);
  i32 fbn = cursor / 512; //printf("fbn: %d\n", fbn);
  i32 numbLeft = numb; //keep track of how much more needs to be read
  i32 numRead = (numb <= 512) ? numb : 512;
  i32 writeStart = 0;
  while (numbLeft > 0){
    numRead = (numbLeft <= 512) ? numbLeft : 512;
    bfsRead(inum,fbn,tempBuf); //bioRead(dbn, buf); 
    paste(buf, tempBuf, writeStart, writeStart + numRead);
    fsSeek(fd, numRead, SEEK_CUR);
    cursor = bfsTell(fd); //printf("cusor %d\n", cursor);
    numbLeft = numbLeft - 512;
    if (numbLeft <= 0){break;}
    fbn ++;
    writeStart = writeStart + numRead;
  }
  //viewBuf(buf);                                  
  return numb;
}

void paste(i8* buf, i8* tempBuf, int start, int end) {
  i32 tempIndex = start % 512;
  /*the buf will be pasted into at next available index in buf, but you should
  copy from first vaild tempBuf index*/
  //printf("start: %d\n",start);
  //printf("end: %d\n", end);
  //printf("tempIndex: %d\n", tempIndex);
  for(int i = start; i <= end; i++){
    buf[i] = tempBuf[tempIndex];
    tempIndex++;
  }
}

void transplant(i8* recipient, i8* donor, int recipientStart, int donorStart, int recipientEnd) {
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

void viewBuf(i8* buf){
  printf("\tbuf: \n");
  //size_t numElements = sizeof(buf) / sizeof(buf[0]);
  for(int i = 0; i < 100; i++){
    printf("%d,",buf[i]);
    //printf("%d\t%d\n",buf[i],i);
  }
  printf("\n");
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
  
  printf("Numb: %d\n", numb);
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd); printf("cusor %d\n", cursor);
  i32 writeStart = cursor % 512; printf("start %d\n", writeStart);
  i32 fbn = cursor / 512; //printf("fbn: %d\n", fbn);
  i32 numbLeft = numb; //keep track of how much more needs to be written
  i32 numWritten = (numb <= 512) ? numb : 512; printf("\tnumWritten: %d\n", numWritten);
  if (writeStart != 0){
    //printf("\toffset on start\n");

    i32 dbn = bfsFbnToDbn(inum, fbn); printf("dbn: %d\n", dbn);
    //printf("fbn(1): %d\n", fbn);
    bfsRead(inum, fbn, tempBuf);
    //viewBuf(tempBuf);
    i32 blockFront = fbn * 512; printf("blockFront: %d\n", blockFront);
    transplant(tempBuf, buf, writeStart, 0, numWritten - writeStart);
    //printf("fbn(2): %d\n", fbn);
    fsSeek(fd, blockFront, SEEK_SET);
    i32 cursor = bfsTell(fd); printf("cusor %d\n", cursor);
    bioWrite(dbn, tempBuf);
    
    //("\tnumWritten: %d\n", numWritten);
    //numWritten = (numb <= 512) ? numb : 512;
    numbLeft = numbLeft - numWritten;// printf("numbLeft: %d\n", numbLeft);

    fsSeek(fd, blockFront + numWritten + writeStart, SEEK_SET);
    cursor = bfsTell(fd); printf("cusor %d\n", cursor);
    fbn++;
    //viewBuf(leftBuf);
    
  }
  while (numbLeft >= 512){
    if (numbLeft <= 0){break;}
    //numWritten = (numbLeft <= 512) ? numbLeft : 512;
    //bfsRead(inum,fbn,tempBuf); //bioRead(dbn, buf); 
    //paste(buf, tempBuf, writeStart, writeStart + numRead);
    i32 dbn = bfsFbnToDbn(inum, fbn);
    cursor = bfsTell(fd); printf("cusor %d\n", cursor);
    transplant(tempBuf, buf, 0, cursor, 512); //gather block from larger buffer
    bioWrite(dbn, tempBuf);
    //writeStart = writeStart + numRead;
    //numWritten = numWritten - 512;
    fbn ++;
    fsSeek(fd, 512, SEEK_CUR);
    numbLeft = numbLeft - 512;
  }
  // if (numbLeft > 0){
  //   printf("end segment\n");
  //   i32 dbn = bfsFbnToDbn(inum, fbn);
  //   bfsRead(inum, fbn, tempBuf);
  //   viewBuf(tempBuf);
  //   cursor = bfsTell(fd); //printf("cusor %d\n", cursor);
  //   transplant(tempBuf, buf, 0, cursor, numbLeft); //gather block from larger buffer
  //   viewBuf(tempBuf);
  //   bioWrite(dbn, tempBuf);
  //   //writeStart = writeStart + numRead;
  //   //numWritten = numWritten - 512;
  //   fbn ++;
  //   fsSeek(fd, 512, SEEK_CUR);
  //   numbLeft = numbLeft - 512;
  // }                              
  return 0;
}
