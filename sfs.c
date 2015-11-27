#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
///////FS ARCHITECTURE////////////////////
//Boot Block 		 =1
//Super Block        =1
//inode table block  =4
//Block Size 		 =512
//No. of blocks      =100
//Max File Length    =14
//Inode Cache not implmented
//security settings not implemented
//journaling not implemented
//Directory Support not implemented
/////////////definitions/////////////////

#define BLOCKSIZE 512     									//block size
#define MAXBLOCK 100										//No. of blocks in device 	
#define MAXSYSSIZE BLOCKSIZE*MAXBLOCK                       //system size 																	
#define MAXBOOTBLOCK 1										//No. of Boot Blocks
#define MAXSUPERBLOCK 1										//No. of Super Blocks
#define MAXINODETABLEBLOCK 4
#define MAXDATABLOCK (MAXBLOCK -(MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK))
#define MAXLINK 13
#define FILESYSNAME "sfs"
#define MAXFNAME 14
#define BOOTADDRESS 0
#define SUPADDRESS (MAXBOOTBLOCK)*BLOCKSIZE
#define INODETABLEADDRESS (MAXBOOTBLOCK+MAXSUPERBLOCK)*BLOCKSIZE
#define DATAADDRESS (MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK)*BLOCKSIZE
#define DATABLOCKSTART MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK
#define MAXDIRENTRYPPAGE (BLOCKSIZE)/sizeof(struct direntry)
#define PAGECOMP ((MAXINODETABLEBLOCK*BLOCKSIZE)%sizeof(struct inode))
#define MAXINODE (MAXINODETABLEBLOCK*BLOCKSIZE)/(sizeof(struct inode))
#define INITIALPAGEALLOC 2
#define MAXOPENOBJECT 2*MAXINODE


//////////used in denoting type of file in inode attribute i_type
#define SDIR 1 		//directory
#define SFILE 2 	//file
#define SEMPT 3		//empty
////////used in writedirentry function/////////////////////////////// 
#define FSEARCH 0  	//searching a directory for direntry with given name 
#define FALLOC 1  	//searching a directory for free direntry index for creating a new direntry for file/directory
///////////////used in seekFile function///////////////////////
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
////////////////////////////////////////////////////////////////



extern int pwdhandle;
struct inode {
	unsigned int	i_size;
	unsigned int	i_atime;
	unsigned int	i_ctime;
	unsigned int	i_mtime;
	unsigned short	i_blks[MAXLINK]; //data block addressess
	short		i_mode;
	unsigned char	i_uid;
	unsigned char	i_gid;
	unsigned char	i_type;
	unsigned char	i_lnk;
};

struct super {
	char sb_vname[MAXFNAME];
	int	sb_nino;
	int	sb_nblk;
	int	sb_blksize;
	int	sb_nfreeblk;
	int	sb_nfreeino;
	int	sb_flags;
	unsigned short sb_freeblks[MAXDATABLOCK];
	int	sb_freeblkindex;
	int	sb_freeinoindex;
	unsigned int	sb_chktime;
	unsigned int	sb_ctime;
	unsigned short sb_freeinos[MAXINODE];
};

struct direntry
{
	char d_name[MAXFNAME];
	unsigned short d_ino;
	int d_offset;
};

struct openobject 
{
	char			ofo_fname[MAXFNAME];
	int				ofo_inode;       //make it a pointer to Incore Inode object when cache is implemented
	int				ofo_mode;
	unsigned int	ofo_curpos;
	int				ofo_ref;
};

/////////used to implement LRU cache by Double Linked List of used inodes////////
// struct InCoreINode {
// 	struct INode 	ic_inode;
// 	int 		ic_ref;
// 	int		ic_ino;
// 	short		ic_dev;
// 	struct InCoreINode	*ic_next;
// 	struct InCoreINode	*ic_prev;
// };


///filesystem related/////////
int mkfs(int fd);
int mount(int fd);

////debug//////////////////////
void displaySuper(struct super *s);
void displayInode(struct inode *r);
void displayOpenobject();
void dispalyDirentry(struct direntry *de);
void displayFileContents(int handle); 
/////file operations interface//////////////////////
int createFile(int dirhandle, char *fname, int mode, int uid, int gid);
int openFile(int dirhandle,char *fname,int mode, int uid, int gid);
int readFile(int fhandle, char buf[], int nbytes);
int writeFile(int fhandle, char buf[], int nbytes);
int closeFile(int fhandle);
int SeekFile(int fhandle, int pos, int whence);


////utilities////////////////////////////////
int max(int l,int r);
int min(int l,int r);
int genearteDataBlockAdress(int d_no);
int generateInodeIndexAdress(int i_no);
void initInode(struct inode *node,int type);

////Device Driver level//////////////////////
struct direntry* searchFile(int fhandle,char *fname);
int writeSuper(struct super *tsb);
int writeInode(int i_no,struct inode *troot);
int writeDirentry(int dirhandle,struct direntry *de,int flag);

////////////////Main menu driven function////////////
int main(int argc,char *argv[])
{
	int ret;
	char buf[512];

	while(1){
	printf(">>>> ");
	scanf("%s",buf);
	if(strcmp(buf,"mkfs") == 0)
	{
		scanf("%s",buf);
		int fd=open(buf,O_RDWR);
		if(fd < 0)
			perror("inappropiate usage\n");
		else
		{
			ret=mkfs(fd);
			if(ret == 0)
				printf("success sfs creation\n");
			else perror("error\n");
		}
	}
	else if(strcmp(buf,"mount") == 0)
	{
		scanf("%s",buf);
		int fd=open(buf,O_RDWR);
		if(fd < 0)
			perror("inappropiate usage\n");
		else
		{
			ret=mount(fd);
			if(ret == 0)
				printf("success sfs Mounting.\n");
			else perror("error\n");
		}
	}
	else if(strcmp(buf,"touch") == 0)
	{
		scanf("%s",buf);
		ret=createFile(pwdhandle,buf,0,0,0);
		if(ret == 0)
		{
			printf("success file create\n");
		}
		else
		{
			perror("error\n");
		}
	}
	else if(strcmp(buf,"ls") == 0)
	{
		displayFileContents(0);
	}
	else if(strcmp(buf,"show") == 0)
	{
		displayOpenobject();
	}
	else if(strcmp(buf,"search") == 0)
	{
		scanf("%s",buf);
		struct direntry *de=searchFile(0,buf);
		if(de != NULL)
		{
			printf("success file search. inode val: %d\n",de->d_ino);
		}
		else
		{
			perror("error.File not Foud\n");
		}
	}
	else if(strcmp(buf,"open") == 0)
	{
		scanf("%s",buf);
		int handle=openFile(pwdhandle,buf,0,0,0);
		if(handle > -1)
		{
			printf("success file opening. FileHandle val: %d\n",handle);
		}
		else
		{
			perror("error.File opening error\n");
		}
	}
	else if(strcmp(buf,"write") == 0)
	{
		char writebuf[2*BLOCKSIZE];
		bzero(writebuf,2*BLOCKSIZE);
		int handle=-1;
		scanf("%d %s",&handle,writebuf);
		ret=writeFile(handle,writebuf,strlen(writebuf));
		if(ret > -1)
		{
			printf("success file Writing. Retvalue val: %d\n",ret);
		}
		else
		{
			perror("error.File Writing error\n");
		}
	}
	else if(strcmp(buf,"read") == 0)
	{
		char readbuf[2*BLOCKSIZE];
		bzero(readbuf,2*BLOCKSIZE);
		int handle=-1;
		int size=-1;
		scanf("%d %d",&handle,&size);
		ret=readFile(handle,readbuf,size);
		if(ret > -1)
		{
			printf("success file Reading. Read string: %s %d\n",readbuf,ret);
		}
		else
		{
			perror("error.File Reading error\n");
		}
	}
	else if(strcmp(buf,"close") == 0)
	{
		int ret=-1;
		int handle = -1;
		scanf("%d",&handle);
		ret=closeFile(handle);
		if(ret > -1)
		{
			printf("success file Close. Return value: %d\n",ret);
		}
		else
		{
			perror("error.File Closing error\n");
		}
	}
	else printf("invalid command\n");
	}
	return 0;
}


//////////Implementation//////////////////////////
int devfd=-1;          							//Storage device file descriptor
struct inode inodetable[MAXINODE];				//inode table	
struct direntry debuffer[MAXDIRENTRYPPAGE];     //direntry buffer
struct openobject obbuffer[MAXOPENOBJECT];      //openObject Buffer
struct super sb;								//superblock in main memeory loaded during mount and synced
int loaded=0;									//denotes whether a device is mounted or not
int pwdhandle = 0; 								//Initial Working directory inode = 0 i.e rot directory


int mkfs(int fd)
{
	char *buf;
	int i;
	////////////fill datablocks with zero///////////
	lseek(fd,DATAADDRESS,SEEK_SET);
	buf=(char *)malloc(BLOCKSIZE);
	bzero(buf,BLOCKSIZE);
	for(i=DATABLOCKSTART;i<MAXBLOCK;i++)write(fd,buf,BLOCKSIZE);


	/////write dummy boot block////
	lseek(fd,BOOTADDRESS,SEEK_SET);
	buf=(char *)malloc(sizeof(char)*BLOCKSIZE*MAXBOOTBLOCK);
	bzero(buf,BLOCKSIZE*MAXBOOTBLOCK);
	write(fd,buf,BLOCKSIZE*MAXBOOTBLOCK);
	///////////////////////////////

	//////Initialise Super block////
	strcpy(sb.sb_vname,FILESYSNAME);
	sb.sb_nino=MAXINODE;
	sb.sb_nfreeino=MAXINODE;
	printf("free block %d\n",MAXDATABLOCK);
	sb.sb_nblk=MAXDATABLOCK;
	sb.sb_nfreeblk=MAXDATABLOCK;
	sb.sb_blksize=BLOCKSIZE;
	sb.sb_flags=0;
	for(i=0;i<MAXDATABLOCK;i++)
		sb.sb_freeblks[i]=0;
	for(i=0;i<MAXINODE;i++)
		sb.sb_freeinos[i]=0;
	sb.sb_freeinos[0]=1;
	sb.sb_nfreeino--;
	sb.sb_chktime=sb.sb_ctime=0;

	/////////initialise rootdirectory inode///////
	initInode(&inodetable[0],SDIR);
	for(i=0;i<MAXDIRENTRYPPAGE;i++)
	{
		strcpy(debuffer[i].d_name,"");
		debuffer[i].d_ino=0;
		debuffer[i].d_offset=0;
	}

	for(i=0;i<INITIALPAGEALLOC;i++)
	{
		int adr=genearteDataBlockAdress(inodetable[0].i_blks[i]);
		lseek(fd,adr,SEEK_SET);
		write(fd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE); //writting dummy direntry array
	}
	////write root directory inode a.k.a 0th entry of inodetable-- inodetable Array //////////////
	lseek(fd,INODETABLEADDRESS,SEEK_SET);
	write(fd,inodetable,sizeof(struct inode)*MAXINODE);
	//////filling remaining portion of inodetable with zero/////////// //some calculation error
	// buf=(char *)malloc(PAGECOMP);
	// bzero(buf,PAGECOMP);
	// write(fd,bzero,PAGECOMP);

	////write super block//////////// // to be written last
	buf=(char *)malloc((MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	bzero(buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	lseek(fd,SUPADDRESS,SEEK_SET);
	write(fd,&sb,sizeof(struct super));
	write(fd,buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	/////////////////////////////////////////////////////////////////
	return 0;
}

int mount(int fd)
{
	int i;
	devfd=fd;
	loaded=1;
	/////////////Load Super Block into main memory sb structure///////////
	lseek(devfd,SUPADDRESS,SEEK_SET);
	read(devfd,&sb,sizeof(struct super));
	/////////Load Inode Table into inodetable Array in main memeory///////
	lseek(devfd,INODETABLEADDRESS,SEEK_SET);
	read(devfd,inodetable,sizeof(struct inode)*MAXINODE);
	////////////Displaying Super Block Contents////////////
	displaySuper(&sb);
	//////////Displaying Root Directory Inode///////////////
	displayInode(&inodetable[0]);
	///////////Initilaising Open Object Buffer//////////////
	for(i=0;i<MAXOPENOBJECT;i++)
	{
		obbuffer[i].ofo_inode=0;
		obbuffer[i].ofo_ref=0;
		obbuffer[i].ofo_curpos=0;
		obbuffer[i].ofo_mode=0;
	}
	return 0;
}

///////////////////////DRBUG DISPLAY//////////////////////////////////

void displaySuper(struct super *s)
{
	printf("Displaying Super Block------------------\n");
	printf("filesys name      :   %s\n",s->sb_vname);
	printf("filesys freeblocks:   %d\n",s->sb_nfreeblk);
	printf("filesys freeinos  :   %d\n",s->sb_nfreeino);
	printf("filesys blocksize :   %d\n",s->sb_blksize);

}
void displayInode(struct inode *r)
{
	printf("Displaying Inode Block------------------\n");
	printf("inode size     :   %d\n",r->i_size);
	printf("Type of inode  :   %s\n",(r->i_type == SDIR)?"directory":"file");
	printf("DataBlocks allocated: ");
	int i;
	if(r->i_type == SDIR || r->i_type == SFILE)
	{
		for(i=0;i<MAXLINK;i++)
			printf("%d %c",r->i_blks[i]," \n"[i==(MAXLINK-1)]);
	}
	else printf("not allocated yet.\n");
}

void displayFileContents(int handle)  //dispalys file content based on file or directory
{
	if( inodetable[handle].i_type == SDIR)
	{
		printf("Dispalying directory contents-------------\n");
		printf("Directory Inode no: %d\n",handle);
		int cnt=0,i=0,fl=0;
		for(cnt=0;cnt < MAXLINK && inodetable[handle].i_blks[cnt] > 0;cnt++)
		{
			int adr=genearteDataBlockAdress(inodetable[handle].i_blks[cnt]);
			lseek(devfd,adr,SEEK_SET);
			read(devfd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE);

			for(i=0;i<MAXDIRENTRYPPAGE;i++)
			{
				if(debuffer[i].d_ino > 0)
				{
					fl=1;
					if(inodetable[debuffer[i].d_ino-1].i_type == SDIR)
					{
						printf("Directory name: %s ---- inode_no: %d----Offset: %d\n",debuffer[i].d_name,debuffer[i].d_ino,debuffer[i].d_offset);
					}
					else if(inodetable[debuffer[i].d_ino-1].i_type == SFILE)
					{
						printf("File name: %s ----------- inode_no: %d----Offset: %d\n",debuffer[i].d_name,debuffer[i].d_ino,debuffer[i].d_offset);
					}
					else
					{
						printf("Error direntry.\n");
					}
				}
			}
		}
		if( !fl)
			printf("directory empty. No records Found\n");
	}
	else printf("not implemented yet\n");
	return;
}


void displayOpenobject()
{
	int i;
	for(i=0;i<MAXOPENOBJECT;i++)
	{
		if(obbuffer[i].ofo_inode > 0)
		{
			printf("Displaying OpenObject Buffer-- Index no: %d------------------------\n",i);
			printf("File Name %s\n",obbuffer[i].ofo_fname);
			printf("Inode No. %d\n",obbuffer[i].ofo_inode);
			printf("File Offset %d\n",obbuffer[i].ofo_curpos);
		}
	}
}

void dispalyDirentry(struct direntry *de)
{
	if(de!=NULL)
	{
		printf("Displaying Direntry contents:=========================== \n");
		printf("Directory Name : %s Inode_No: %d d_offset: %d \n",de->d_name,de->d_ino,de->d_offset);
		printf("============================================================================\n");
	}
	else printf("=======================Direntry contents is empty. =========================== \n");
}//////////////////////////////////////////////////////////////////////////////



int getFreeInodeIndex()
{
	int i;
	for(i=0;i<MAXINODE;i++)
	{
		if(sb.sb_freeinos[i] == 0)
			return i+1;
	}
	return -1;
}

int writeSuper(struct super *tsb)
{
	lseek(devfd,SUPADDRESS,SEEK_SET);
	write(devfd,tsb,sizeof(struct super));
	return 0;
}

int writeInode(int i_no,struct inode *troot)
{
	int adr=generateInodeIndexAdress(i_no);
	lseek(devfd,adr,SEEK_SET);
	write(devfd,troot,sizeof(struct inode));
	return 0;
}

int writeDirentry(int dirhandle,struct direntry *de,int flag)//finds a slot and write the entry
{
	unsigned int i=0,j=0,adr;
	for(i=0;i<MAXLINK && inodetable[dirhandle].i_blks[i]>0;i++)
	{
		adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[i]);
		lseek(devfd,adr,SEEK_SET);
		read(devfd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE);
		for(j=0;j<MAXDIRENTRYPPAGE;j++)
		{
			if( flag == FALLOC && debuffer[j].d_ino == 0)
			{
				///alloctae this direntry slot///
				printf("allocating slot in direntry table: %d\n",j);
				strcat(debuffer[j].d_name,de->d_name);
				debuffer[j].d_ino=de->d_ino;
				debuffer[j].d_offset=de->d_offset;
				printf("written at address %d\n",adr+j*(int)sizeof(struct direntry));
				lseek(devfd,adr,SEEK_SET);
				write(devfd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE);
				return 0;
			}
			else if(flag == FSEARCH && strcmp(debuffer[j].d_name,de->d_name) == 0)
			{
				///search and replace this direntry slot///
				printf("Search and replace  slot in direntry table: %d\n",j);
				strcpy(debuffer[j].d_name,de->d_name);
				debuffer[j].d_ino=de->d_ino;
				debuffer[j].d_offset=de->d_offset;
				printf("written at address %d\n",adr+j*(int)sizeof(struct direntry));
				lseek(devfd,adr,SEEK_SET);
				write(devfd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE);
				return 0;
			}
		}
	}
	return -1;//no place found //allocate new data block and write //Add later
}

int createFile(int dirhandle, char *fname, int mode, int uid, int gid)
{
	printf("create file called......\n");
	displayInode(&inodetable[dirhandle]);
	displayFileContents(dirhandle);
	//////////////////////////////////////////////////
	struct direntry *ret=searchFile(dirhandle,fname);
	if( ret != NULL)
	{
		printf("returned value %d\n",ret->d_ino);
		printf("File already exists in this directory.Try another name.\n");
		return -1;
	}

	int inodeindex=getFreeInodeIndex();
	printf("free inode obtained is: %d\n",inodeindex);
	initInode(&inodetable[inodeindex-1],SFILE);
	displayInode(&inodetable[inodeindex-1]);
	////write Altered Inode  to Disk//////
	writeInode(inodeindex,&inodetable[inodeindex-1]);
	sb.sb_freeinos[inodeindex-1]=1;
	sb.sb_nfreeino--;

	struct direntry de;
	strcpy(de.d_name,fname);
	de.d_ino=inodeindex;
	de.d_offset=0;
	writeDirentry(dirhandle,&de,FALLOC);

	////write Altered Super block to Disk//////
	writeSuper(&sb);
	return 0;
}

struct direntry* searchFile(int dirhandle,char *fname)
{
	int i,j;
	struct direntry *de=(struct direntry*)malloc(sizeof(struct direntry));
	for(i=0;i<MAXLINK && inodetable[dirhandle].i_blks[i] >0;i++)
	{
		int adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[i]);
		lseek(devfd,adr,SEEK_SET);
		read(devfd,debuffer,sizeof(struct direntry)*MAXDIRENTRYPPAGE);
		for(j=0;j<MAXDIRENTRYPPAGE;j++)
		{
			if(strcmp(debuffer[j].d_name,fname) == 0) //match of direntry name in this particular directory
			{
				strcpy(de->d_name,debuffer[j].d_name);
				de->d_ino=debuffer[j].d_ino;
				de->d_offset=debuffer[j].d_offset;
				return de;
			}
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////


int getOpenobjectIndex()
{
	int i;
	for(i=0;i<MAXOPENOBJECT;i++)
	{
		if(obbuffer[i].ofo_inode == 0)
			return i;
	}
	return -1;
}

int allocateOpenobject(struct direntry *de,int mode)
{
	int ooindex=getOpenobjectIndex(); //openobject index
	if(de != NULL && ooindex != -1)
	{
		strcpy(obbuffer[ooindex].ofo_fname,de->d_name);
		obbuffer[ooindex].ofo_inode=de->d_ino;
		obbuffer[ooindex].ofo_curpos=de->d_offset;
		obbuffer[ooindex].ofo_mode=mode;
		obbuffer[ooindex].ofo_ref=0;
	}
	else perror("No free space to allocate new openobject\n");
	return ooindex;
}

int openFile(int dirhandle,char *fname,int mode, int uid, int gid)
{
	struct direntry *de=searchFile(dirhandle,fname);
	dispalyDirentry(de);
	return allocateOpenobject(de,mode);
}


int readFile(int fhandle, char buf[], int nbytes) //fhandle = open oject buufer index
{
	printf("numbytes: %d\n",nbytes);
	int inodeindex=obbuffer[fhandle].ofo_inode-1;
	char fbuff[BLOCKSIZE];
	bzero(fbuff,BLOCKSIZE);
	bzero(buf,sizeof(buf)/sizeof(char));

	if( nbytes > MAXLINK*BLOCKSIZE)return -1;

	int nb=nbytes;
	int cnt=0,i=0,j,size=0;
	while(nb > 0 && size <= inodetable[inodeindex].i_size)
	{
		j=0;
		int m=min(nb,BLOCKSIZE);
		//m=min(inodetable[inodeindex].i_size,m);
		int adr=genearteDataBlockAdress(inodetable[inodeindex].i_blks[cnt++]);
		printf("reading inode: %d from file: %s numbytes: %d \n",inodeindex,obbuffer[fhandle].ofo_fname,m);
		printf("Reading From address %d\n",adr);
		lseek(devfd,adr,SEEK_SET);
		read(devfd,fbuff,m);
		while(i<m)
			buf[i++]=fbuff[j++];
		size+=m;
		nb-=BLOCKSIZE;
	}
	printf("wihin read file %s\n",buf);

	//////////raw read for test///////////////////////////////////////
	// int adr=genearteDataBlockAdress(inodetable[inodeindex].i_blks[0]);
	// printf("reading from file inode no: %d address: %d \n",inodeindex,adr);
	// lseek(devfd,adr,SEEK_SET);
	// int size=read(devfd,buf,nbytes);
	// printf("within readfile buffer content %s\n",buf);
	return size;
}



int writeFile(int fhandle, char buf[], int nbytes) //fhandle = open oject buffer index
/**
*FIXME: Extend this for O_APPEND mode
*/
{
	int inodeindex=obbuffer[fhandle].ofo_inode-1;

	/////////only overwrite from begining is allowed//////////
	char fbuff[BLOCKSIZE];
	if( nbytes > MAXLINK*BLOCKSIZE)return -1;

	int cnt=0,j=0,i=0;
	printf("writing inode: %d from file: %s\n",inodeindex,obbuffer[fhandle].ofo_fname);

	while(i<nbytes)
	{
		j=0;
		while(i<nbytes && j<BLOCKSIZE)
			fbuff[j++]=buf[i++];
		int adr=genearteDataBlockAdress(inodetable[inodeindex].i_blks[cnt++]);
		printf("Writing Data at address %d\n",adr);
		lseek(devfd,adr,SEEK_SET);
		write(devfd,fbuff,j);
	}
	inodetable[inodeindex].i_size=nbytes;
	obbuffer[fhandle].ofo_curpos=nbytes;


	printf("write success----------------->\n");
	displayOpenobject();
	displayInode(&inodetable[inodeindex]);
	return nbytes;
}



int closeFile(int fhandle) //fhandle = open oject buufer index
/*Simultaneous file operations LOGIC:



*/
{
	int ret=-1;
	if(obbuffer[fhandle].ofo_inode > 0)//if valid fhandle
	{
		//////////////write corresponding directory entry to disk////////////
		struct direntry tde;
		strcpy(tde.d_name,obbuffer[fhandle].ofo_fname);
		tde.d_ino = obbuffer[fhandle].ofo_inode;
		tde.d_offset = obbuffer[fhandle].ofo_curpos;
		ret=writeDirentry(pwdhandle,&tde,FSEARCH);
		if(ret == -1)return ret;

		//////////Make necessary changes to Inode /////////////
		inodetable[obbuffer[fhandle].ofo_inode -1].i_size=max(obbuffer[fhandle].ofo_curpos,inodetable[obbuffer[fhandle].ofo_inode -1].i_size);

		///////////Freeing the obbuffer////////
		strcpy(obbuffer[fhandle].ofo_fname,"");
		obbuffer[fhandle].ofo_inode = 0;
		obbuffer[fhandle].ofo_curpos = 0;
		obbuffer[fhandle].ofo_mode = 0;
		obbuffer[fhandle].ofo_ref = 0;
		return  0;
	}
	else return -1;
}


int seekFile(int fhandle, int pos, int whence) //fhandle = open oject buufer index
{
	switch(whence)
	{
		case SEEK_SET:
			break;
		case SEEK_CUR:
			break;
		case SEEK_END:
		default:
			printf("Invalid flag whence\n");
			return -1;
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////
int max(int l,int r)
{
	return (l>r)?l:r;
}

int min(int l,int r)
{
	if(l<r)return l;
	else return r;
}

inline int genearteDataBlockAdress(int d_no)
{
	return (MAXBOOTBLOCK + MAXSUPERBLOCK + MAXINODETABLEBLOCK+ d_no-1)*BLOCKSIZE;
}

inline int generateInodeIndexAdress(int i_no)
{
	return (MAXBOOTBLOCK + MAXSUPERBLOCK)*BLOCKSIZE + (i_no-1)*sizeof(struct inode);
}

void initInode(struct inode *node,int type)
{
	int i,j,cnt=0;
	node->i_size=0;
	node->i_atime=node->i_ctime=node->i_mtime=0;
	node->i_mode=node->i_uid=node->i_gid=0;
	node->i_type=type; //type of inode: dir/file/empty
	node->i_lnk=1;
	for(i=0;i<MAXLINK;i++)node->i_blks[i]=0;
	if( type == SDIR || type == SFILE) //if SEMP do not allocate data blocks
	{
		for(i=0;i<MAXDATABLOCK && cnt<INITIALPAGEALLOC;i++)  //initially alocated 2 data blocks
		{									  //for directory and files
			if(sb.sb_freeblks[i] == 0)
			{
				node->i_blks[cnt++]=i+1;
				printf("Allocating block no. %d\n",i+1);
				sb.sb_freeblks[i]=1;
				sb.sb_nfreeblk--;
			}
		}
	}
}