#include <stdio.h>
#include <string.h>
#include "file_utils.h"

char *get_content_type(const char *filename){
    //char content_type[32];
    const char *ext = strrchr(filename, '.')+1;
    if(!ext || ext == filename) return "";

    if((strcmp(ext, "htm") == 0) || (strcmp(ext, "html")==0)){
        return "text/html";
    }else if(strcmp(ext, "css") ==0){
        return "text/css";
    }else if(strcmp(ext, "js") == 0){
        return "application/javascript";
    } 
    return "";
    //return content_type;
}

off_t find_file_size(FILE *fp){
    off_t f_size;
    if(fseeko(fp, 0, SEEK_END) == 0){
        f_size = ftello(fp);
        fseeko(fp, 0, SEEK_SET);
        if(f_size == -1){
            perror("Cannot get the size of the file by ftello()");
            return -1;
            }
    }else{
        perror("Cannot find SEEK_END by fseeko()");
        return -1;
    }
    return f_size;
}

