#ifndef WEB_SERVER_H_INCLUDED
#define WEB_SERVER_H_INCLUDED

#include <vector>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

class filecount_struct{
    public:
    char *file;
    int count;
};

class web_param {
    public:
    std::vector <filecount_struct*> *vc;
    char *html_path;
    char *folder_path;
    bool restart=false;
    time_t html_folder_last_modified;
    web_param(char *folder_path, char *html_path);
    ~web_param();
    void copy_count(void);
};

int create_fastcgi_threads(char *ip_addr, web_param param);

std::vector <filecount_struct*>* add_files_to_vector(const char* folder_path);

#endif // WEB_SERVER_H_INCLUDED
