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

#define BLOCKSIZE 512
#define MAXBLOCK 100
#define MAXSYSSIZE BLOCKSIZE*MAXBLOCK

#define MAXFD 10
#define MAXBOOTBLOCK 1
#define MAXSUPERBLOCK 1
#define MAXINODETABLEBLOCK 4
#define MAXDATABLOCK (MAXBLOCK -(MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK))
#define MAXLINK 13
#define FILESYSNAME "sfs"
#define MAXFNAME 14
#define SDIR 1
#define SFILE 2
#define SEMPT 3
#define BOOTADDRESS 0
#define SUPADDRESS (MAXBOOTBLOCK)*BLOCKSIZE
#define INODETABLEADDRESS (MAXBOOTBLOCK+MAXSUPERBLOCK)*BLOCKSIZE
#define DATAADDRESS (MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK)*BLOCKSIZE
#define DATABLOCKSTART MAXBOOTBLOCK+MAXSUPERBLOCK+MAXINODETABLEBLOCK
#define MAXDIRENTRYPPAGE (BLOCKSIZE-sizeof(struct direntrytop))/sizeof(struct direntry)
#define PAGECOMP ((MAXINODETABLEBLOCK*BLOCKSIZE)%sizeof(struct inode))
#define MAXINODE (MAXINODETABLEBLOCK*BLOCKSIZE)/(sizeof(struct inode))


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
};
struct direntrytop
{
	char d_name[MAXFNAME];
	unsigned short d_ino;
	unsigned short d_num;
	//unsigned short d_table[MAXDIRENTRYPPAGE];  //index for direntries in directory
												 // implement later
};


///filesystem related/////////
int mkfs(int fd);
int loadfs(int fd);

////debug//////////////////////
void displaySuper(struct super *s);
void displayInode(struct inode *r);

/////file operations interface//////////////////////
int createFile(int dirhandle, char *fname, int mode, int uid, int gid);
int openfile(char *fname,int mode, int uid, int gid);
int writeFile(int filehandle,int mode, int uid, int gid);


////utilities////////////////////////////////
int genearteDataBlockAdress(int d_no);
int generateInodeIndexAdress(int i_no);
void initInode(struct inode *node,int type);

////Device Driver level//////////////////////
int writeSuper(struct super *tsb);
int writeInode(int i_no,struct inode *troot);
int findFreeDirentry(int dirhandle,struct direntry *de);
int findDirentry(int dirhandle,char *fname);
int writeDirentry(int dirhandle,struct direntry *de);
int readDirentrytop(int dirhandle,struct direntrytop *de);
int writeDirentrytop(int dirhandle,struct direntrytop *de);



////////////////Main menu driven function////////////
int main(int argc,char *argv[])
{
	printf("%d",(int)sizeof(struct direntry));
	printf("%d",(int)sizeof(struct direntrytop));

	int ret;
	char buf[512];

	while(1){
	scanf("%s",buf);	
	if(strcmp(buf,"create") == 0)
	{
		scanf("%s",buf);	
		int fd=open(buf,O_RDWR);
		if(fd < 0)
		{
			perror("inappropiate usage\n");
		}
		else
		{
			ret=mkfs(fd);
			if(ret == 0)
			{
				printf("success sfs creation\n");
			}
			else
			{
				perror("error\n");
			}
		}
	}
	else if(strcmp(buf,"load") == 0)
	{
		scanf("%s",buf);
		int fd=open(buf,O_RDWR);
		if(fd < 0)
		{
			perror("inappropiate usage\n");
		}
		else
		{
			ret=loadfs(fd);
			if(ret == 0)
			{
				printf("success sfs loading\n");
			}
			else
			{
				perror("error\n");
			}
		}
	} 
	else if(strcmp(buf,"createfile") == 0)
	{
		scanf("%s",buf);
		ret=createFile(0,buf,0,0,0);
		if(ret == 0)
		{
			printf("success file create\n");
		}
		else
		{
			perror("error\n");
		}
	}
	}
	return 0;
}


//////////Implementation//////////////////////////
int devfd=-1;
unsigned short descriptor[MAXFD];
struct inode inodetable[MAXINODE];
struct super sb;
int loaded=0;


int genearteDataBlockAdress(int d_no)
{
	return (MAXBOOTBLOCK + MAXSUPERBLOCK + MAXINODETABLEBLOCK+ d_no-1)*BLOCKSIZE;
}

int generateInodeIndexAdress(int i_no)
{
	return (MAXBOOTBLOCK + MAXSUPERBLOCK)*BLOCKSIZE + (i_no-1)*sizeof(struct inode);
}

void initInode(struct inode *node,int type)
{
	int i,cnt=0;
	node->i_size=sizeof(struct inode);
	node->i_atime=node->i_ctime=node->i_mtime=0;
	node->i_mode=node->i_uid=node->i_gid=0;
	node->i_type=type; //type of inode: dir/file/empty
	node->i_lnk=0; 
	for(i=0;i<MAXLINK;i++)node->i_blks[i]=0;
	if( type == SDIR || type == SFILE) //if SEMP do not allocate data blocks
	{
		for(i=0;i<MAXDATABLOCK && cnt<2;i++)  //initially alocated 2 data blocks
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
	struct inode troot;
	initInode(&troot,SDIR);
	struct direntrytop tde;
	strcpy(tde.d_name,"root");
	tde.d_ino=1;
	tde.d_num=1;
	int adr=genearteDataBlockAdress(troot.i_blks[0]);
	lseek(fd,adr,SEEK_SET);
	write(fd,&tde,sizeof(struct direntrytop));

	struct direntry de; //dummy entry
	strcpy(de.d_name,"");
	de.d_ino=0;
	for(i=0;i<MAXDIRENTRYPPAGE -1;i++) write(fd,&de,sizeof(struct direntry)); //only filling one page with dummy entries

	////write root inode a.k.a 0th entry of inodetable////////
	lseek(fd,INODETABLEADDRESS,SEEK_SET);
	write(fd,&troot,sizeof(struct inode));
	////write other inode table entries//////
	for(i=0; i< MAXINODE -1;i++)
	{
		initInode(&troot,SEMPT);
		write(fd,&troot,sizeof(struct inode));
	}
	//////filling remaining portion of inodetable with zero///////////
	buf=(char *)malloc(PAGECOMP);
	bzero(buf,PAGECOMP);
	write(fd,bzero,PAGECOMP);

	////write super block//////////// // to be written last
	buf=(char *)malloc((MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	bzero(buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	lseek(fd,SUPADDRESS,SEEK_SET);
	write(fd,&sb,sizeof(struct super));
	write(fd,buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	/////////////////////////////////////////////////////////////////
	close(fd);
	return 0;
}

int loadfs(int fd)
{
	int i;
	lseek(fd,SUPADDRESS,SEEK_SET);	
	read(fd,&sb,sizeof(struct super));
	lseek(fd,INODETABLEADDRESS,SEEK_SET);
	for(i=0;i<MAXINODE;i++)read(fd,&(inodetable[i]),sizeof(struct inode));
	displaySuper(&sb);
	displayInode(&inodetable[0]);
	devfd=fd;
	loaded=1;
	return 0;
}

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
void displayFileContents(int handle)  //dispalys file content based on file or directory
{
	if( inodetable[handle].i_type == SDIR)
	{
		printf("Dispalying directory contents-------------\n");
		int cnt=0,i=0,rec=0;
		struct direntrytop det;
		struct direntry de;
		while(cnt < MAXLINK && inodetable[handle].i_blks[cnt] > 0)
		{
			rec=0;
			int adr=genearteDataBlockAdress(inodetable[handle].i_blks[cnt]);
			lseek(devfd,adr,SEEK_SET);
			if(cnt == 0)
			{
				if(( read(devfd,&det,sizeof(struct direntrytop)) == sizeof(struct direntrytop) ))
				{
					printf("Directory name: %s ----------- inode_no: %d -------Num of subfiles: %d\n",det.d_name,det.d_ino,det.d_num);
				}
				i++;
				rec++;
			}

			while( i< det.d_num && rec < MAXDIRENTRYPPAGE  && read(devfd,&de,sizeof(struct direntry))) //potential error
			{
				printf("recno %d ino: %d\n",rec,de.d_ino);
				if(de.d_ino > 0)
				{
					if(inodetable[de.d_ino-1].i_type == SDIR)
					{
						printf("Directory name: %s ----------- inode_no: %d\n",de.d_name,de.d_ino);
					}
					else if(inodetable[de.d_ino-1].i_type == SFILE)
					{
						printf("File name: %s ----------- inode_no: %d\n",de.d_name,de.d_ino);
					}
					else
					{
						printf("Error direntry.\n");
					}
					i++;
					if(i>= det.d_num)
						return;
				}
				rec++;
			}
			cnt++;
		}
	}
	else printf("not implemented yet\n");
	return;
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

int writeDirentry(int dirhandle,struct direntry *de)//finds a slot and write the entry
{
	int i=0,j=0;
	struct direntry tde;
	for(i=0;inodetable[dirhandle].i_blks[i]>0;i++)
	{
		int adr=0;
		if(i==0)
		{	
			adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[i]);
			adr+=sizeof(struct direntrytop);
			lseek(devfd,adr,SEEK_SET);
			int cnt=0;
			for(j=0;j<MAXDIRENTRYPPAGE -1 ;j++)
			{
				read(devfd,&tde,sizeof(struct direntry));
				if(tde.d_ino == 0)
				{
					lseek(devfd,adr+cnt*sizeof(struct direntry),SEEK_SET);
					write(devfd,de,sizeof(struct direntry));
					int t=adr+ cnt*sizeof(struct direntry);
					printf("written at address %d\n",t);
					return 0;
				}
				else cnt++;
			}
		}
		else
		{
			adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[i]);
			lseek(devfd,adr,SEEK_SET);
			int cnt=0;
			for(j=0;j<MAXDIRENTRYPPAGE;j++)
			{
				read(devfd,&tde,sizeof(struct direntry));
				if(tde.d_ino == 0)
				{
					lseek(devfd,adr+cnt*sizeof(struct direntry),SEEK_SET);
					write(devfd,de,sizeof(struct direntry));
					return 0;
				}
				else cnt++;
			}
		}
	}
}
int readDirentrytop(int dirhandle,struct direntrytop *de)
{
	int adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[0]);
	lseek(devfd,adr,SEEK_SET);
	read(devfd,de,sizeof(struct direntrytop));
	return 0;
}
int writeDirentrytop(int dirhandle,struct direntrytop *de)
{
	int adr=genearteDataBlockAdress(inodetable[dirhandle].i_blks[0]);
	lseek(devfd,adr,SEEK_SET);
	write(devfd,de,sizeof(struct direntrytop));
	return 0;
}
int findDirentry(int dirhandle,char *fname);
{

}
int writeDirentry(int dirhandle,struct direntry *de)
{

}


int createFile(int dirhandle, char *fname, int mode, int uid, int gid)
{
	printf("create file called\n");
	displayFileContents(dirhandle);
	int inodeindex=getFreeInodeIndex();
	printf("free inode obtained is: %d\n",inodeindex);
	initInode(&inodetable[inodeindex-1],SFILE);
	displayInode(&inodetable[inodeindex-1]);
	writeInode(inodeindex,&inodetable[inodeindex-1]);
	sb.sb_freeinos[inodeindex-1]=1;
	sb.sb_nfreeino--;


	struct direntrytop tde;
	struct direntry de;
	strcpy(de.d_name,fname);
	de.d_ino=inodeindex;
	writeDirentry(dirhandle,&de);

	readDirentrytop(dirhandle,&tde);
	printf("read direntrytop: %s %d %d \n",tde.d_name,tde.d_ino,tde.d_num);
	tde.d_num+=1;
	writeDirentrytop(dirhandle,&tde);
	displayFileContents(dirhandle);
	writeSuper(&sb);
	return 0;	
}
int openfile(int direhandle,char *fname,int mode, int uid, int gid)
{
	struct direntrytop tde;
	readDirentrytop(dirhandle,&tde);
	for(int i=0;;i++)
	{

	}
}
int writeFile(int filehandle,int mode, int uid, int gid)
{

}
int readFile(int filehandle,int mode, int uid, int gid)
{

}



