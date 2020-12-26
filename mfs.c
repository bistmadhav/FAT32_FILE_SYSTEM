

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_NUM_ARGUMENTS 6
#define NUM_ENTRIES 16
#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE  0x20


struct  __attribute__(( __packed__ )) DirectoryEntry
{
 char DIR_Name[11];
 uint8_t DIR_Attr;
 uint8_t Unused1[8];
 uint16_t DIR_FirstClusterHigh;
 uint8_t Unused2[4];
 uint16_t DIR_FirstClusterLow;
 uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

int16_t BPB_BytesPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATs;
int32_t BPB_FATz32;
FILE *fp;
int file_open = 0;

int32_t LBAToOffset (int32_t sector)
{
	return((sector-2)+BPB_BytesPerSec) +(BPB_NumFATs +BPB_FATz32+BPB_BytesPerSec)+(BPB_RsvdSecCnt+BPB_BytesPerSec);

}

int16_t NextLB( int32_t sector)
{
 uint32_t  FATAddress = (BPB_BytesPerSec+BPB_RsvdSecCnt)+ (sector+4);
int16_t val;
fseek(fp, FATAddress, SEEK_SET);
fread(&val, 2, 1, fp);
return val;
}

int compare(char * userString, char *directoryString)
{
char *dotdot = "..";
if(strncmp(dotdot , userString, 2)==0)
{ 
 if(strncmp(userString, directoryString, 2)==0)
{
return 1;

}
return 0;
}
char IMG_Name[12];
strncpy(IMG_Name, directoryString, 11);
IMG_Name[11] = '\0';
char input[11];
memset(input, 0, 11);
strncpy(input, userString, strlen(userString));
char expended_name[12];
memset(expended_name, ' ', 12);
char *token = strtok(input, ".");
strncpy(expended_name, token, strlen(token));
token = strtok(NULL, ".");
if(token)
{


strncpy( (char*) (expended_name+8), token , strlen(token));
}
expended_name[11]= '\0';
int i;
for(i=0; i<11; i++)
{
expended_name[i] = toupper(expended_name[i]);
}
if(strncmp(expended_name, IMG_Name, 11)==0)
{
return 1; 

}
return 0;

}


int bpb()
{
printf("BPB_BytesPerSec: %d 0x%x", BPB_BytesPerSec, BPB_BytesPerSec);
printf("BPB_SecPerClus: %d 0x%x", BPB_SecPerClus, BPB_SecPerClus);
printf("BPB_RsvdSecCnt:%d 0x%x", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
printf("BPB_NumFATS: %d 0x%x", BPB_NumFATs, BPB_NumFATs);
printf("BPB_FATs32: %d 0x%x", BPB_FATz32, BPB_FATz32);

return 0;
}





int ls( )
{
int i;
for(i=0; i<NUM_ENTRIES; i++)
{
char filename[12];
strncpy(filename, dir[i].DIR_Name, 11);
filename[11] = '\0';
if((dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr==ATTR_DIRECTORY || dir[i].DIR_Attr==ATTR_ARCHIVE ) && filename[0]!=0xffffffe5)
{
printf("%s\n", filename);

}
}
return 0;
}



int cd (char *dictionaryName)
{
int i;
int found =0;
for(i=0;i<NUM_ENTRIES;i++)
{
if(compare(dictionaryName, dir[i].DIR_Name))
{
int cluster = dir[i].DIR_FirstClusterLow;

if(cluster ==0)
{
  cluster = 2;

}
int offset = LBAToOffset(cluster);
fseek(fp, offset, SEEK_SET);
fread(dir, sizeof(struct DirectoryEntry), NUM_ENTRIES, fp);
found =1;
break;

}

}
if(!found)
{
 printf("Error: Directory not found\n");
 return -1;

}

return 0;

}


int statFile(char *filename)
{

int i =0;
int found =0;
for(i=0;i<NUM_ENTRIES;i++)
{
if(compare(filename, dir[i].DIR_Name))
{
printf("%s Attr: %d, Size: %d Cluster: %d\n", filename, dir[i].DIR_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
found =1;

}

}
if(!found)
{
printf("Error:File Not Found\n");

}

return 0;
}



int getFile(char *originalFilename, char *newFilename)
{
FILE *ofp;
if(newFilename==NULL)
{
ofp = fopen(newFilename, "w");
if(ofp==NULL)
{

printf("Couldn't open the file\n");

}

}
else 
{
ofp = fopen(newFilename, "w");
if(ofp==NULL)
{
printf("Unable to open the file\n");

}

}
int i;
int found =0;

for(i =0; i<NUM_ENTRIES; i++)
{
if(compare(originalFilename, dir[i].DIR_Name))
{
int cluster = dir[i].DIR_FirstClusterLow;

found =1;
int bytesReaminingToRead = dir[i].DIR_FileSize;
int offset = 0;
unsigned char buffer[512];

while(bytesReaminingToRead == BPB_BytesPerSec)
{
offset = LBAToOffset(cluster);
fseek(fp, offset, SEEK_SET);
fread(buffer, 1, BPB_BytesPerSec, fp);
fwrite(buffer, 1, 512, ofp);

cluster = NextLB(cluster);
bytesReaminingToRead = bytesReaminingToRead- BPB_BytesPerSec;


}
if(bytesReaminingToRead)
{
offset = LBAToOffset(cluster);
fseek(fp, offset, SEEK_SET);
fread(buffer, 1, bytesReaminingToRead, fp);
fwrite(buffer, 1, bytesReaminingToRead, ofp);
}

fclose(ofp);
}

}

}




int readFile(char *filename, int requestedOffset, int requestedBytes)

{
 int i;
int found =0;
int bytesReamaningToRead = requestedBytes;

if(requestedOffset<0)
{
printf("Error Offset can not be less than 0");

}

for(i=0; i<NUM_ENTRIES; i++)
{
if(compare(filename, dir[i].DIR_Name))
{
int cluster = dir[i].DIR_FirstClusterLow;
found = 1;

int searchSize = requestedOffset;
while(searchSize >=BPB_BytesPerSec)
{
cluster  = NextLB(cluster);
searchSize = searchSize-BPB_BytesPerSec;
}
int offset = LBAToOffset(cluster);
int byteOffset = (requestedOffset %BPB_BytesPerSec);
fseek(fp, offset+byteOffset, SEEK_SET);
unsigned char buffer[BPB_BytesPerSec];


int firstBlockBytes = BPB_BytesPerSec-requestedOffset;
fread(buffer, 1, firstBlockBytes, fp);
for(i =0; i<firstBlockBytes; i++)
{
printf("%s\n", buffer[i]);
}
bytesReamaningToRead = bytesReamaningToRead - firstBlockBytes;

while(bytesReamaningToRead>=512)

{
cluster = NextLB(cluster);
offset = LBAToOffset(cluster);
fseek (fp, offset, SEEK_SET);
fread(buffer, 1, BPB_BytesPerSec, fp);

for(i =0; i<BPB_BytesPerSec; i++)
{
printf("%s\n", buffer[i]);

}
bytesReamaningToRead = bytesReamaningToRead - BPB_BytesPerSec;

}

if(bytesReamaningToRead)
{
cluster = NextLB(cluster);
offset = LBAToOffset(cluster);
fseek(fp, offset, SEEK_SET);
fread(buffer,1,bytesReamaningToRead, fp);

for(i =0; i<bytesReamaningToRead; i++)
{

printf("%x", buffer[i]);


}
}
printf("\n");
}
}
if(!found)
{

printf("Error:File Not Found\n");
return -1;

}
return 0;
}





int main()

{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  
  
	
  while( 1 )
  {
     

      printf("msf>");
    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }
    if(token[0] ==NULL)
    {
	continue;

    }

    else if(strcmp(token[0], "quit")==0)
     {
	
	exit(0);

     }

    else if(strcmp(token[0], "open")==0)
   {
	fp = fopen(token[1], "r");
        if(fp == NULL)
        {
		printf("The given file is null\n");

        }
        else 
       {
		file_open = 1;

       }
     
	fseek(fp, 11, SEEK_SET);
        fread(&BPB_BytesPerSec, 1,2,fp);
        fseek(fp, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1,1,fp);
	fseek(fp, 14, SEEK_SET);   
	fread(&BPB_RsvdSecCnt, 1,2,fp);
        fseek(fp, 16, SEEK_SET);
        fread(&BPB_NumFATs, 1,2,fp);
        fseek(fp, 36, SEEK_SET);
	fread(&BPB_FATz32,1,4,fp);
       int root_Address = BPB_RsvdSecCnt * BPB_BytesPerSec +(BPB_NumFATs * BPB_FATz32 * BPB_BytesPerSec);
         fseek(fp, root_Address, SEEK_SET);
          fread(dir, sizeof(struct DirectoryEntry), 16, fp);
      }
    else if(strcmp(token[0], "close")==0)

    {
	if(file_open)
        {
	  fclose(fp);
          fp = NULL;
          file_open =0;

        }
       else 

         {
		printf("Error the file is open\n");

         }
    }

  else if( strcmp(token[0], "bpb")==0)

  {
   if(file_open)
   {
	bpb();

   }
   else 
  {
	printf("Error unable to open the filr\n");

  }

  }
   else if(strcmp(token[0], "ls")==0)
  {

	if(file_open)
        {

		ls();

        }
	else 
       {
		printf("Error the file is closed\n");

        }


  }


    else if( strcmp(token[0], "cd")==0)
   {
	if(file_open)
        {
		cd(token[1]);
        }
        else 

       {
		printf("File NOT open\n");
       }

   }

   else if(strcmp(token[0], "read")==0)
  {
	if(file_open)
       {
	   readFile(token[1], atoi(token[2]), atoi(token[3]));

       }
       else 
      {
	printf("The file is closed error\n");

       }


  }

   else if(strcmp(token[0], "get")==0)

    { 
	if(file_open)
        {
		getFile(token[1], token[2]);																																												
        }
     else 

     {

	printf("Error has occured\n");

     }

    }

   else if(strcmp(token[0], "stat")==0)

   {
      if(file_open)
      {
	  statFile(token[1]);	
      }
     else 

     {

	printf("The erro has  occured\n");

     }

   }
    
    free( working_root );

  }
  return 0;
} 
 

