// #########################################################################

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <include/portable_endian.h>

// #########################################################################

int saveFile(const char*fileName,uint8_t*X,size_t fs)
{
    int rv=0;
    FILE*hf=fopen(fileName,"wb");
    if(hf)
    {
        if(fwrite(X,1,fs,hf)==fs)
        {
            rv=1;
        }
        fclose(hf);
    }
    return rv;
}

// #########################################################################

size_t loadFile(const char*fileName,uint8_t*X)
{
    size_t fs=0;
    FILE*hf=fopen(fileName,"rb");
    if(hf)
    {
        fseek(hf,0,SEEK_END);
        fs=ftell(hf);
        if(X)
        {
            fseek(hf,0,SEEK_SET);
            if(fread(X,1,fs,hf)!=fs)
            {
                fs=0;
            }
        }
        fclose(hf);
    }
    return fs;
}

// #########################################################################
// dd if=/dev/mapper/root-crypt | gzip | hexdump -v -e '64/1 "%02x""\n"'
int dehex(const char*in,const char*out)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        printf("input: %s, fs: %zu\n",in,fs);
        uint8_t b_in[fs+16];
        uint8_t b_out[fs+16];
        uint8_t b_tmp[4*1024];
        fs=loadFile(in,b_in);
        size_t outLen=0;
        size_t lineLen=0;
        uint8_t octet=0,value=0;
        int fileNumber=0;
        for(size_t i=0;i<fs;++i)
        {
            uint8_t c=b_in[i];
            switch(c)
            {
            case 'A'...'F':
                c=c-'A'+'a';
            case 'a'...'f':
                c=c-'a'+'0'+10;
            case '0'...'9':
                c=c-'0';
                octet=1-octet;
                value=(value<<4)|(c&0xf);
                if(octet==0)
                {
                    b_tmp[lineLen++]=value;
                }
                break;
            case '\n':
                if(lineLen)
                {
                    memcpy(b_out+outLen,b_tmp,lineLen);
                    outLen+=lineLen;
                }
            case ':':
                lineLen=0;
            case ' ':
                octet=0;
                break;
            default:
                if(outLen)
                {
                    char fout[1024];
                    sprintf(fout,"%s_%i.bin",out,++fileNumber);
                    printf("output: %s, fs: %zu\n",fout,outLen);
                    saveFile(fout,b_out,outLen);
                    outLen=0;
                }
                lineLen=0;
                octet=2;
                break;
            }
        }
    }
    return 0;
}

// #########################################################################

int checksum(const char*in,int fix)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        printf("input: %s, fs: %zu\n",in,fs);
        uint8_t data[fs+3];
        fs=loadFile(in,data);
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);

        if((fs<32)||(memcmp(data+4,"eGON.BT",7)!=0))
        {
            printf("eGON header is not found\n");
            return 1;
        }

        uint32_t l=le32toh(data32[4]);
        if((l>fs)||((l%4)!=0))
        {
            printf("bad length in the eGON header\n");
            return 1;
        }
        l/=4;

        uint32_t c=0x5F0A6C39-le32toh(data32[3]);
        for(uint32_t i=0;i<l;++i)
            c+=le32toh(data32[i]);

        if(c!=le32toh(data32[3]))
        {
            if(fix!=0)
            {
                data32[3]=htole32(c);
                saveFile(in,data,fs);
                printf("checksum updated\n");
                return 0;
            }
            printf("checksum check failed\n");
            return 1;
        }

        printf("checksum OK\n");
        return 0;
    }
    return 1;
}

// #########################################################################

int split(const char*in)
{
    size_t fs=loadFile(in,0);
    if(fs)
    {
        printf("input: %s, fs: %zu\n",in,fs);
        uint8_t data[fs+3];
        fs=loadFile(in,data);
        while(data[fs-1]==0xff)
            --fs;
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);
        uint32_t offs=le32toh(data32[6]);
        saveFile("script.bin",data+offs,fs-offs);
        fs=offs;
        while(data[fs-1]==0xff)
            --fs;
        data32[5]=0;
        data32[6]=0;
        saveFile(in,data,fs);
        return 0;
    }
    return 1;
}

// #########################################################################

int join(const char*in0,const char*in1)
{
    size_t fs0=loadFile(in0,0);
    size_t fs1=loadFile(in1,0);
    if(fs0&&fs1)
    {
        printf("input: %s, fs: %zu\n",in0,fs0);
        printf("input: %s, fs: %zu\n",in1,fs1);
        uint8_t data[fs0*2+fs1];
        fs0=loadFile(in0,data);
        while(fs0&(0x10000-1))
            data[fs0++]=0xff;
        uint32_t*data32=reinterpret_cast<uint32_t*>(data);
        data32[6]=htole32(fs0);
        fs0+=loadFile(in1,data+fs0);
        while(fs0&(0x2000-1))
            data[fs0++]=0xff;
        data32[5]=htole32(fs0);
        saveFile(in0,data,fs0);
        return 0;
    }
    return 1;
}

// #########################################################################

int main(int argc,const char*argv[])
{
    int ret=0;
    for(int i=1;i<argc;++i)
    {
        if(strcmp(argv[i],"dehex")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"capture";
            const char*out=(argc>(i+1))?(++i,argv[i]):"dehexout";
            ret+=dehex(in,out);
        }
        if(strcmp(argv[i],"check")==0)
        {
            const int fix=((argc>(i+1))&&(strcmp(argv[i+1],"-f")==0))?++i:0;
            const char*in=(argc>(i+1))?(++i,argv[i]):"fes1.bin";
            ret+=in?checksum(in,fix):1;
        }
        if(strcmp(argv[i],"split")==0)
        {
            const char*in=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            ret+=split(in);
        }
        if(strcmp(argv[i],"join")==0)
        {
            const char*in0=(argc>(i+1))?(++i,argv[i]):"uboot.bin";
            const char*in1=(argc>(i+1))?(++i,argv[i]):"script.bin";
            ret+=join(in0,in1);
        }
    }
}

// #########################################################################
