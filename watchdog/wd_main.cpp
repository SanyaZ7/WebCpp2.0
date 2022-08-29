
#include <iostream>
#include <string>
#include <string.h>
#include <fcntl.h> ///O_RDONLY
#include <unistd.h> ///read, unlink
#include <sys/stat.h> //mkfifo
#include <thread>
using namespace std;

char* get_workspace_folder_path(char* argv0)
{
    char *str=strdup(argv0);
    int len=strlen(argv0);
    int nesting_level=1;
    while(nesting_level!=0||len!=0)
    {
        if(str[len]=='/') --nesting_level;
        if(nesting_level==0) {str[len+1]=0;break;}
        --len;
    }
    return str;
}

int run_web_app(char *folder_path)
{
    char terminal_name[]="xfce4-terminal";
    string path{folder_path};
    path.append("bin/Release/WebCpp2");
    path.insert(0, " --command=");
    path.insert(0, terminal_name);
    //cout<<"1"<<endl;
    //cout<<path<<endl;
    system(path.c_str());
    //cout<<"2"<<endl;
    return 0;
}

int io_func(char *folder_path, bool *restart)
{
    string path1{folder_path};
    path1.append("watchdog/");
    path1.append("wd_to_app");
    string path2{folder_path};
    path2.append("watchdog/");
    path2.append("app_to_wd");
    mkfifo(path1.c_str(), 0666);
    mkfifo(path2.c_str(), 0666);
    int fd1 = open(path1.c_str(), O_WRONLY);
    int fd2 = open(path2.c_str(), O_RDONLY);
    if(fd1 < 0){perror("open error"); return 1;}
    if(fd2 < 0){perror("open error"); return 1;}
    ///чтение канала
    int i=0;
    while(1)
    {
        char buf_write[]="Работаешь?";
        char buf_read[64]={0};
        write(fd1, buf_write, strlen(buf_write));
        ssize_t read_size = read(fd2, buf_read, sizeof(buf_read) - 1);
        cout<<buf_write<<endl;
        if(!strcmp(buf_read, "Да"))
        {
            i++;
            cout<<"i="<<i<<endl;
            cout<<"Хорошо"<<endl;
        }
        else
        {
            cout<<buf_read<<endl;
            *restart=true;
            close(fd1); close(fd2); unlink(path1.c_str()); unlink(path2.c_str());
             return 0;
        }
        sleep(2);
    }
}

int main(int argc, char *argv[])
{
    ///создание веб
    char* folder_path=get_workspace_folder_path(argv[0]);

    start:
    bool restart=true;
    std::thread thr1=std::thread(run_web_app, folder_path);
    std::thread thr2=std::thread(io_func, folder_path, &restart);
    thr1.join();
    thr2.join();
    if(restart==true) goto start;
    return 0;
}

