
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include "../web_server.h"

#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>

#include "fastcgi.h"
#include "fcgiapp.h"
#include "fcgimisc.h"

using namespace std;
using namespace std::chrono;

#define min_(a,b) ((a) < (b) ? (a) : (b))

std::string urlencode(const std::string &s)
{
    static const char lookup[]= "0123456789abcdef";
    std::stringstream e;
    for(int i=0, ix=s.length(); i<ix; i++)
    {
        const char& c = s[i];
        if ( (48 <= c && c <= 57) ||//0-9
             (65 <= c && c <= 90) ||//abc...xyz
             (97 <= c && c <= 122) || //ABC...XYZ
             (c=='-' || c=='_' || c=='.' || c=='~')
        )
        {
            e << c;
        }
        else
        {
            e << '%';
            e << lookup[ (c&0xF0)>>4 ];
            e << lookup[ (c&0x0F) ];
        }
    }
    return e.str();
}

std::string urlDecode(std::string &SRC) {
    std::string ret;
    char ch;
    int ii;
    for (unsigned long i=0; i<SRC.length(); i++) {
        if (int(SRC[i])==37) {
            sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else {
            ret+=SRC[i];
        }
    }
    return (ret);
}

void post_and_get_requests(FCGX_Request *request, char *uri, web_param *param)
{
    if(!strcmp(uri, "/content.html"))
    {
        printf("REQUEST_METHOD=%s\n", FCGX_GetParam("REQUEST_METHOD", request->envp));
        printf("REQUEST_URI=%s\n", FCGX_GetParam("REQUEST_URI", request->envp));
        printf("DOCUMENT_URI=%s\n", FCGX_GetParam("DOCUMENT_URI", request->envp));
        int ilen = atoi(FCGX_GetParam("CONTENT_LENGTH", request->envp));
        char *bufp = (char*) malloc(ilen+1);
        memset(bufp, 0, ilen+1);
        FCGX_GetStr(bufp, ilen, request->in);
        std::string str(bufp);
        printf("The POST data is<P>%s\n", urlDecode(str).c_str());
        free(bufp);
    }
}

int fcgi_write_from_file(FCGX_Request *request, std::ifstream &istrm)
{
    istrm.seekg (0, istrm.end);
    std::streampos size = istrm.tellg();
    istrm.seekg (0, istrm.beg);

    request->out=NewWriter(request, static_cast<int>(size)+8, FCGI_STDOUT);

    FCGX_PutS("\n", request->out);
    if(size <= (request->out->stop - request->out->wrNext))
    {
        ///memcpy(request->out->wrNext, memblock, size);
        istrm.read (reinterpret_cast<std::basic_istream<char>::char_type*>(request->out->wrNext), size);
        request->out->wrNext += size;
         return static_cast<int>(size);
    }
    else
    {
        int m, bytesMoved;
        bytesMoved = 0;
        int pos=0;
        for (;;)
        {
            if(request->out->wrNext != request->out->stop)
            {
                m = min_(static_cast<long int>(size) - bytesMoved, request->out->stop - request->out->wrNext);

                //memcpy(request->out->wrNext, str, m);
                istrm.read (reinterpret_cast<std::basic_istream<char>::char_type*>(request->out->wrNext), m);

                bytesMoved += m;
                request->out->wrNext += m;
                if (bytesMoved == static_cast<long int>(size))
                {
                    istrm.close();
                    return bytesMoved;
                }
                //str += m;
                pos=pos+m;
                //istrm.seekg (0, istrm.beg);
                istrm.seekg(pos);
            }
            if(request->out->isClosed || request->out->isReader)
                return bytesMoved;
            request->out->emptyBuffProc(request->out, FALSE);
        }
        /*char *memblock = new char [size];
        istrm.read (memblock, size);
        byte_count=byte_count+FCGX_PutStr(memblock, size, request->out);
        delete[] memblock;*/
    }
    istrm.close();
    return 0;
}

int fcgi_write_from_buffer(FCGX_Request *request, char *buf, int len)
{
    request->out=NewWriter(request, len+8, FCGI_STDOUT);

    FCGX_PutS("\n", request->out);
    if(len <= (request->out->stop - request->out->wrNext))
    {
        memcpy(request->out->wrNext, buf, len);
        ///istrm.read (reinterpret_cast<std::basic_istream<char>::char_type*>(request->out->wrNext), size);
        request->out->wrNext += len;
         return static_cast<int>(len);
    }
    else
    {
        int m, bytesMoved;
        bytesMoved = 0;
        int pos=0;
        for (;;)
        {
            if(request->out->wrNext != request->out->stop)
            {
                m = min_(static_cast<long int>(len) - bytesMoved, request->out->stop - request->out->wrNext);

                memcpy(request->out->wrNext, buf, m);
                bytesMoved += m;
                request->out->wrNext += m;
                if (bytesMoved == len)
                {
                    return bytesMoved;
                }
                buf += m;
            }
            if(request->out->isClosed || request->out->isReader)
                return bytesMoved;
            request->out->emptyBuffProc(request->out, FALSE);
        }
    }
    return 0;
}

int add_all_path_for_send(FCGX_Request *request, char *uri, web_param *param)
{
    if(param->vc!=nullptr)
    for(filecount_struct *fc: *(param->vc))
    {
        ///страница по умолчанию
        char *str_temp="/index.html"; ///временный указатель для index.html
        if(!strcmp(uri, "/"))
        {
            goto start;
        }
        str_temp=fc->file;
        if(!strcmp(uri, str_temp))
        {
            start:
            std::string filename{param->html_path};
            filename+=str_temp;

            std::ifstream istrm(filename, std::ios::binary);
            if (istrm.is_open())
            {
                fc->count++; ///счётчик количества запросов
                std::stringstream extension;
                extension<<std::filesystem::path(filename.c_str()).extension();
                if(extension.str()!=".mstch")
                {
                    fcgi_write_from_file(request, istrm);
                    /*istrm.seekg (0, istrm.end);
                    std::streampos size = istrm.tellg();
                    istrm.seekg (0, istrm.beg);
                    char *memblock = new char [size];
                    istrm.read (memblock, size);
                    fcgi_write_from_buffer(request, memblock, size);
                    delete[] memblock;*/
                    goto end;
                }
                else
                {

                }
            }
            else std::cout << "Unable to open file";
        }
    }
    end:
    post_and_get_requests(request, uri, param);
    return 0;
}

std::vector <filecount_struct*>* add_files_to_vector(const char* html_path)
{
    std::vector <filecount_struct*> *vc = new std::vector<filecount_struct*>;
    const std::filesystem::path sandbox(html_path);
    namespace fs = std::filesystem;
    if(fs::exists(sandbox))
    {
        for(std::filesystem::directory_entry const& dir_entry: std::filesystem::recursive_directory_iterator{sandbox})
        {
            if(fs::is_regular_file(dir_entry))
            {
                filecount_struct *fc=new filecount_struct;
                fc->count=0;
                fc->file=strdup(dir_entry.path().c_str()+strlen(html_path)-1);
                vc->emplace_back(fc);
                cout<<fc->file<<endl;
            }
        }
    }
    return vc;
}

int thread_func(int socketId, web_param *param)
{
     int rc;
    FCGX_Request request;
    if(FCGX_InitRequest(&request, socketId, 0) != 0)
    {
        //ошибка при инициализации структуры запроса
        fprintf(stderr, "Can not init request\n");
        return 0;
    }
    while(!(param->restart))
    {
        static std::mutex accept_mutex;
        //попробовать получить новый запрос
        printf("Try to accept new request\n");
        accept_mutex.lock();
        rc = FCGX_Accept_r(&request);
        param->copy_count();
        accept_mutex.unlock();
        if(rc < 0)
        {
            //ошибка при получении запроса
            fprintf(stderr, "Can not accept new request\n"); break;
        }
        if(rc==0)
        {
            //получить значение переменной
            char *uri = FCGX_GetParam("DOCUMENT_URI", request.envp);
            add_all_path_for_send(&request, uri, param);
        }
        FCGX_Finish_r(&request);
    }
    return 0;
}

int create_fastcgi_threads(char *ip_addr, web_param param)
{
    FCGX_Init();
    int THREAD_COUNT=4;//sysconf(_SC_NPROCESSORS_ONLN);
     //открываем новый сокет
    int socketId = FCGX_OpenSocket(ip_addr, 20); ///20 - глубина очереди
     if(socketId < 0)
    {
        fprintf(stderr, "Failed to open socket with FCGX_OpenSocket\n");
        return -1;
    }
    std::thread thr[THREAD_COUNT];
    for(int i=0; i<THREAD_COUNT; ++i)
    {
        thr[i]=std::thread(thread_func, socketId, &param);
    }
    for(int i=0; i<THREAD_COUNT; ++i)
    {
        thr[i].join();
    }
    return 0;
}

web_param::web_param(char *folder_path, char *html_path)
{
    this->folder_path=strdup(folder_path);
    this->html_path=strdup(html_path);
    this->vc=add_files_to_vector(html_path);
    struct stat attr;
    stat(this->html_path, &attr);
    this->html_folder_last_modified=attr.st_mtime;
}

web_param::~web_param()
{
    for(filecount_struct *fc: *(this->vc))
    {
        free(fc->file);
        delete fc;
    }
    free(folder_path);
    free(html_path);
}

void web_param::copy_count(void)
{
    struct stat attr;
    stat(this->html_path, &attr);
    time_t time=attr.st_mtime;
    if(time>this->html_folder_last_modified)
    {
        std::vector <filecount_struct*> *vc2=add_files_to_vector(this->html_path);
        for(filecount_struct *fc2: *vc2)
        {
            for(filecount_struct *fc: *(this->vc))
            {
               if(!strcmp(fc2->file, fc->file))
               {
                    fc2->count=fc->count;
               }
            }
        }
        for(filecount_struct *fc: *(this->vc))
        {
            free(fc->file);
            delete fc;
        }
        delete this->vc;
        this->vc=vc2;
        this->html_folder_last_modified=time;
    }
}
