
#include <cstring>
#include <string>
#include <../web_server.h>
#include <thread>
#include <fcntl.h> ///O_WRONLY, O_RDONLY
#include <sys/stat.h> //mkfifo
#include <unistd.h> //unlink

using namespace std;

char* get_workspace_folder_path(char* argv0)
{
    char *str=strdup(argv0);
    int len=strlen(argv0);
    int nesting_level=3;
    while(nesting_level!=0||len!=0)
    {
        if(str[len]=='/') --nesting_level;
        if(nesting_level==0) {str[len+1]=0;break;}
        --len;
    }
    return str;
}

int write_wd(char *argv0, web_param *param)
{
    char* folder_path=get_workspace_folder_path(argv0);
    string path1{folder_path};
    path1.append("watchdog/");
    path1.append("wd_to_app");
    string path2{folder_path};
    path2.append("watchdog/");
    path2.append("app_to_wd");
    int fd1 = open(path1.c_str(), O_RDONLY);
    int fd2 = open(path2.c_str(), O_WRONLY);
    if(fd1 < 0){perror("open error"); return 1;}
    if(fd2 < 0){perror("open error"); return 1;}
    ///чтение канала
    while(!(param->restart))
    {
        char buf_write[]="Да";
        char buf_read[64]={0};
        ssize_t read_size = read(fd1, buf_read, sizeof(buf_read) - 1);
        printf("buf_write=%s\n", buf_write);
        if(!strcmp(buf_read, "Работаешь?"))
        {write(fd2, buf_write, strlen(buf_write));}
    }
    close(fd1); close(fd2);
    return 0;
}

int input(char *argv0, char *folder_path, char* terminal_name)
{
    string path{folder_path};
    path.append("/watchdog/wdapp");
    path.insert(0, " --command=");
    path.insert(0, terminal_name);
    system(path.c_str());
    return 0;
}

int main(int argc, char *argv[])
{
    int kkk=55;

    char *folder_path=get_workspace_folder_path(argv[0]);
    std::string html_path{folder_path};
    html_path.append("html/");
    web_param param(folder_path, (char*) html_path.c_str());
    std::thread thr=std::thread(write_wd, argv[0], &param);
    create_fastcgi_threads("127.0.0.1:8000", param);
    thr.join();
    free(folder_path);
    return 0;
}
