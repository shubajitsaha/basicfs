#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
///////FS ARCHITECTURE////////////////////
//Boot Block 		 =1
//Super Block        =1
//inode table block  =4 
/////////////definitions/////////////////

struct inode;
struct super;


#define BLOCKSIZE 512
#define MAXBLOCK 100
#define MAXSYSSIZE BLOCKSIZE*MAXBLOCK

#define MAXDEVICE 10
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
#define MAXDIRENTRYPPAGE BLOCKSIZE/sizeof(struct direntry)

struct inode {
	unsigned int	i_size;
	unsigned int	i_atime;
	unsigned int	i_ctime;
	unsigned int	i_mtime;
	unsigned short	i_blks[13]; //data block addressess
	short		i_mode;
	unsigned char	i_uid;
	unsigned char	i_gid;
	unsigned char	i_type;
	unsigned char	i_lnk;
};
#define PAGECOMP ((MAXINODETABLEBLOCK*BLOCKSIZE)%sizeof(struct inode))
#define MAXINODE (MAXINODETABLEBLOCK*BLOCKSIZE)/(sizeof(struct inode))
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
};



int mkfs(int fd);
int loadfs(int fd);
void displaySuper(struct super *s);
void displayInode(struct inode *r);
int createFile(int dirhandle, char *fname, int mode, int uid, int gid);

////////////////Main menu driven function////////////
int main(int argc,char *argv[])
{
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
	for(i=0;i<MAXLINK;i++)node->i_blks[i]=0;
	if( type == SDIR || type == SFILE) //if SEMP do not allocate data blocks
	{
		for(i=0;i<MAXDATABLOCK && cnt<2;i++)  //initially alocated 2 data blocks
		{									  //for directory and files
			if(sb.sb_freeblks[i] == 0)
			{
				node->i_blks[cnt++]=i+1;
				sb.sb_freeblks[i]=1;
				sb.sb_nfreeblk--;
			}
		}
	}
	node->i_mode=node->i_uid=node->i_gid=0;
	node->i_type=type; //type of inode: dir/file/empty
	node->i_lnk=0; 
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
	struct super tsb;
	strcpy(tsb.sb_vname,FILESYSNAME);
	tsb.sb_nino=MAXINODE;
	tsb.sb_nfreeino=MAXINODE;
	printf("free block %d\n",MAXDATABLOCK);
	tsb.sb_nblk=MAXDATABLOCK;
	tsb.sb_nfreeblk=MAXDATABLOCK;
	tsb.sb_blksize=BLOCKSIZE;
	tsb.sb_flags=0;
	for(i=0;i<MAXDATABLOCK;i++)
		tsb.sb_freeblks[i]=0;
	for(i=0;i<MAXINODE;i++)
		sb.sb_freeinos[i]=0;
	tsb.sb_freeinos[0]=1;
	tsb.sb_nfreeino--;
	tsb.sb_chktime=sb.sb_ctime=0;

	////write super block////////////
	buf=(char *)malloc((MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	bzero(buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));
	lseek(fd,SUPADDRESS,SEEK_SET);
	write(fd,&tsb,sizeof(struct super));
	write(fd,buf,(MAXSUPERBLOCK*BLOCKSIZE)-sizeof(struct super));

	/////////initialise rootdirectory inode///////
	struct inode troot;
	initInode(&troot,SDIR);
	struct direntrytop de;
	strcpy(de.d_name,"root");
	de.d_ino=1;
	de.d_num=1;
	int adr=genearteDataBlockAdress(troot.i_blks[0]);
	lseek(fd,adr,SEEK_SET);
	write(fd,&de,sizeof(struct direntrytop));
	strcpy(de.d_name,"");
	de.d_ino=0;
	de.d_num=0;
	for(i=0;i<MAXDIRENTRYPPAGE -1;i++) write(fd,&de,sizeof(struct direntrytop));

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
	printf("DataBlocks allocated:\n");
	int i;
	if(r->i_type == SDIR || r->i_type == SDIR)
	{
		for(i=0;i<MAXLINK;i++)
		{
			if(r->i_blks[i] != 0)
				printf("%d ",r->i_blks[i]);
		}
		printf("\n");
	}
}

int getFreeInodeIndex()
{
	int i;
	for(i=0;i<MAXINODE;i++)
	{
		if(sb.sb_freeinos[i] == 0)
		{
			sb.sb_freeinos[i]=1;
			return i+1;
		}
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

			while( i< det.d_num && rec < MAXDIRENTRYPPAGE  && read(devfd,&de,sizeof(struct direntry)) == sizeof(struct direntry)) //potential error
			{
				printf("recno %d\n",rec);
				if(de.d_ino > 0)
				{
					if(inodetable[de.d_ino].i_type == SDIR)
					{
						printf("Directory name: %s ----------- inode_no: %d\n",de.d_name,de.d_ino);
					}
					else if(inodetable[de.d_ino].i_type == SFILE)
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

int writeSuper(struct inode *tsb)
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

int writeDirentry(int dirhandle,struct direntry de)
{

}
int createFile(int dirhandle, char *fname, int mode, int uid, int gid)
{
	printf("create file called\n");
	displayFileContents(dirhandle);
	int inodeindex=getFreeInodeIndex();
	printf("free inode obtained is: %d\n",inodeindex);
	struct troot;
	initInode(&troot,SFILE);
	sb.sb_freeinos[inodeindex-1]=1;
	sb.sb_nfreeino--;

	struct direntry de;
	strcpy(de.d_name,fname);
	de.d_ino=inodeindex;
	writeDirentry(dirhandle,&de);
	writeInode(inodeindex,&troot);

	return 0;	
}



